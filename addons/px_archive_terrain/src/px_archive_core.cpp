#include "px_archive_core.h"

#include "../thirdparty/lzma/Alloc.h"
#include "../thirdparty/lzma/LzmaDec.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace px_archive_terrain {
namespace {

size_t bounded_strlen(const char *p_text, size_t p_max_length) {
	for (size_t i = 0; i < p_max_length; i++) {
		if (p_text[i] == '\0') {
			return i;
		}
	}
	return p_max_length;
}

template <typename T>
bool read_pod(std::istream &p_stream, T &r_value) {
	p_stream.read(reinterpret_cast<char *>(&r_value), sizeof(T));
	return p_stream.good();
}

template <typename T>
bool read_value(const uint8_t *&r_cursor, size_t &r_remaining, T &r_value) {
	if (r_remaining < sizeof(T)) {
		return false;
	}
	std::memcpy(&r_value, r_cursor, sizeof(T));
	r_cursor += sizeof(T);
	r_remaining -= sizeof(T);
	return true;
}

template <typename T>
bool read_array(const uint8_t *&r_cursor, size_t &r_remaining, T *p_dst, size_t p_count) {
	constexpr size_t elem_size = sizeof(T);
	if (elem_size != 0 && p_count > std::numeric_limits<size_t>::max() / elem_size) {
		return false;
	}
	const size_t bytes = p_count * elem_size;
	if (r_remaining < bytes) {
		return false;
	}
	if (bytes > 0) {
		std::memcpy(p_dst, r_cursor, bytes);
	}
	r_cursor += bytes;
	r_remaining -= bytes;
	return true;
}

std::string build_hot_name(const std::string &p_base_name) {
	return p_base_name + ".hot";
}

std::string build_dem_name(const std::string &p_base_name, int p_lod) {
	return p_base_name + "_LOD" + std::to_string(p_lod) + ".dem";
}

std::string build_texture_name(const std::string &p_base_name, int p_lod) {
	return p_base_name + "_LOD" + std::to_string(p_lod) + ".dds";
}

std::string build_index_name(int p_lod) {
	return "IDX_LOD" + std::to_string(p_lod) + ".idx";
}

int grouped_degree(double p_value) {
	int degree = static_cast<int>(p_value);
	if (p_value < 0.0) {
		degree--;
	}
	while (degree % 5) {
		degree--;
	}
	return degree;
}

std::string format_lat_dir(int p_lat_degree) {
	char buffer[32] = {};
	std::snprintf(buffer, sizeof(buffer), "Lat%02d%c", std::abs(p_lat_degree), p_lat_degree >= 0 ? 'N' : 'S');
	return buffer;
}

std::string format_long_name(int p_long_degree) {
	char buffer[32] = {};
	std::snprintf(buffer, sizeof(buffer), "Long%03d%c", std::abs(p_long_degree), p_long_degree >= 0 ? 'E' : 'W');
	return buffer;
}

} // namespace

bool ArchiveReader::open(const std::string &p_archive_path) {
	close();

	std::ifstream stream(p_archive_path, std::ios::binary);
	if (!stream.is_open()) {
		error_text = "Failed to open archive: " + p_archive_path;
		return false;
	}

	uint32_t format = 0;
	int64_t largest_compressed_size = 0;
	int64_t largest_uncompressed_size = 0;
	int32_t element_count = 0;
	if (!read_pod(stream, format) ||
			!read_pod(stream, largest_compressed_size) ||
			!read_pod(stream, largest_uncompressed_size) ||
			!read_pod(stream, element_count) ||
			element_count < 0 ||
			largest_compressed_size < 0 ||
			largest_uncompressed_size < 0) {
		error_text = "Invalid archive header: " + p_archive_path;
		return false;
	}

	headers.resize(static_cast<size_t>(element_count));
	for (int32_t i = 0; i < element_count; i++) {
		RawFileHeader &header = headers[static_cast<size_t>(i)];
		if (!read_pod(stream, header)) {
			close();
			error_text = "Invalid archive file table: " + p_archive_path;
			return false;
		}
		if (header.offset < 0 ||
				header.compressed_size < 0 ||
				header.uncompressed_size < 0 ||
				header.compressed_size > largest_compressed_size ||
				header.uncompressed_size > largest_uncompressed_size) {
			close();
			error_text = "Invalid archive file entry: " + p_archive_path;
			return false;
		}

		const size_t name_length = bounded_strlen(header.filename, sizeof(header.filename));
		if (name_length == 0 || name_length >= sizeof(header.filename)) {
			continue;
		}
		file_indices.emplace(std::string(header.filename, name_length), static_cast<size_t>(i));
	}

	archive_path = p_archive_path;
	const std::filesystem::path archive_fs_path = std::filesystem::u8path(p_archive_path);
	terrain_root_path = archive_fs_path.parent_path().parent_path().u8string();
	build_candidates();
	error_text.clear();
	(void)format;
	return true;
}

void ArchiveReader::close() {
	archive_path.clear();
	terrain_root_path.clear();
	headers.clear();
	file_indices.clear();
	candidates.clear();
}

bool ArchiveReader::is_open() const {
	return !archive_path.empty();
}

const std::string &ArchiveReader::get_archive_path() const {
	return archive_path;
}

const std::string &ArchiveReader::get_error_text() const {
	return error_text;
}

const std::vector<TileCandidate> &ArchiveReader::get_candidates() const {
	return candidates;
}

bool ArchiveReader::read_tile(const TileCandidate &p_candidate, int p_lod, int p_texture_lod, uint32_t p_max_vertices, TileData &r_tile, bool p_read_texture) {
	if (!is_open()) {
		error_text = "Archive is not open.";
		return false;
	}
	if (p_lod < 0 || p_lod >= TILE_LOD_COUNT) {
		error_text = "Invalid tile LOD.";
		return false;
	}
	if (p_texture_lod < 0 || p_texture_lod >= TILE_LOD_COUNT) {
		error_text = "Invalid texture LOD.";
		return false;
	}

	TileData tile;
	tile.base_name = p_candidate.base_name;
	tile.candidate = p_candidate;

	if (!read_hot(p_candidate.base_name, tile)) {
		return false;
	}
	if (!read_dem(p_candidate.base_name, p_lod, p_max_vertices, tile)) {
		return false;
	}
	if (!read_index(p_lod, static_cast<uint32_t>(tile.dem_points.size()), tile)) {
		return false;
	}
	if (p_read_texture && !read_texture(p_candidate.base_name, p_texture_lod, tile)) {
		tile.has_texture = false;
	}

	r_tile = std::move(tile);
	return true;
}

bool ArchiveReader::build_archive_paths(const std::string &p_terrain_folder, double p_latitude, double p_longitude, ArchivePathSet &r_paths) {
	if (p_terrain_folder.empty()) {
		return false;
	}

	const int lat_degree = grouped_degree(p_latitude);
	const int long_degree = grouped_degree(p_longitude);
	const std::filesystem::path root = std::filesystem::u8path(p_terrain_folder);
	const std::filesystem::path lat_path = root / format_lat_dir(lat_degree);
	const std::string long_name = format_long_name(long_degree);

	r_paths.lat_path = lat_path.u8string();
	r_paths.tile_path = (lat_path / long_name).u8string();
	r_paths.archive_path = (lat_path / (long_name + ".7z")).u8string();
	return true;
}

std::vector<std::string> ArchiveReader::build_neighbor_archive_paths(const std::string &p_terrain_folder, double p_latitude, double p_longitude, int p_neighborhood) {
	std::vector<std::string> paths;
	p_neighborhood = std::max(0, p_neighborhood);

	const int center_lat_degree = grouped_degree(p_latitude);
	const int center_long_degree = grouped_degree(p_longitude);
	for (int lat_offset = -p_neighborhood; lat_offset <= p_neighborhood; lat_offset++) {
		for (int long_offset = -p_neighborhood; long_offset <= p_neighborhood; long_offset++) {
			const double lat = static_cast<double>(center_lat_degree + lat_offset * 5);
			const double lon = static_cast<double>(center_long_degree + long_offset * 5);
			ArchivePathSet built;
			if (build_archive_paths(p_terrain_folder, lat, lon, built)) {
				if (std::find(paths.begin(), paths.end(), built.archive_path) == paths.end()) {
					paths.push_back(built.archive_path);
				}
			}
		}
	}
	return paths;
}

bool ArchiveReader::read_file(const std::string &p_name, std::vector<uint8_t> &r_data) {
	size_t index = 0;
	if (!find_file(p_name, index)) {
		error_text = "Archive entry not found: " + p_name;
		return false;
	}

	const RawFileHeader &header = headers[index];
	std::ifstream stream(archive_path, std::ios::binary);
	if (!stream.is_open()) {
		error_text = "Failed to reopen archive: " + archive_path;
		return false;
	}

	std::vector<uint8_t> compressed(static_cast<size_t>(header.compressed_size));
	r_data.assign(static_cast<size_t>(header.uncompressed_size), 0);
	stream.seekg(header.offset, std::ios::beg);
	stream.read(reinterpret_cast<char *>(compressed.data()), static_cast<std::streamsize>(compressed.size()));
	if (!stream.good()) {
		error_text = "Failed to read archive entry: " + p_name;
		return false;
	}

	SizeT source_size = static_cast<SizeT>(compressed.size());
	SizeT destination_size = static_cast<SizeT>(r_data.size());
	ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
	const SRes result = LzmaDecode(
			reinterpret_cast<Byte *>(r_data.data()),
			&destination_size,
			reinterpret_cast<const Byte *>(compressed.data()),
			&source_size,
			reinterpret_cast<const Byte *>(&header.props),
			LZMA_PROPS_SIZE,
			LZMA_FINISH_ANY,
			&status,
			&g_Alloc);

	if (result != SZ_OK) {
		error_text = "Failed to decompress archive entry: " + p_name;
		return false;
	}

	r_data.resize(static_cast<size_t>(destination_size));
	return true;
}

bool ArchiveReader::read_external_file(const std::string &p_name, std::vector<uint8_t> &r_data) {
	if (terrain_root_path.empty()) {
		return false;
	}

	const std::filesystem::path file_path = std::filesystem::u8path(terrain_root_path) / std::filesystem::u8path(p_name);
	std::ifstream stream(file_path, std::ios::binary | std::ios::ate);
	if (!stream.is_open()) {
		error_text = "File not found in archive or terrain root: " + p_name;
		return false;
	}

	const std::streampos size = stream.tellg();
	if (size < 0) {
		error_text = "Invalid external file size: " + p_name;
		return false;
	}

	r_data.assign(static_cast<size_t>(size), 0);
	stream.seekg(0, std::ios::beg);
	if (!r_data.empty()) {
		stream.read(reinterpret_cast<char *>(r_data.data()), static_cast<std::streamsize>(r_data.size()));
	}
	if (!stream.good() && !stream.eof()) {
		error_text = "Failed to read external file: " + p_name;
		return false;
	}

	return true;
}

bool ArchiveReader::find_file(const std::string &p_name, size_t &r_index) const {
	const auto found = file_indices.find(p_name);
	if (found == file_indices.end()) {
		return false;
	}
	r_index = found->second;
	return true;
}

void ArchiveReader::build_candidates() {
	std::unordered_map<std::string, bool> seen_tiles;
	candidates.clear();

	for (const RawFileHeader &header : headers) {
		const size_t name_length = bounded_strlen(header.filename, sizeof(header.filename));
		if (name_length == 0 || name_length >= sizeof(header.filename)) {
			continue;
		}

		const std::string filename(header.filename, name_length);
		std::string base_name;
		if (filename.size() > 4 && filename.compare(filename.size() - 4, 4, ".hot") == 0) {
			base_name = filename.substr(0, filename.size() - 4);
		} else {
			const size_t lod_pos = filename.find("_LOD");
			if (lod_pos == std::string::npos || filename.size() <= 4 || filename.compare(filename.size() - 4, 4, ".dem") != 0) {
				continue;
			}
			base_name = filename.substr(0, lod_pos);
		}

		if (seen_tiles.find(base_name) != seen_tiles.end()) {
			continue;
		}

		TileCandidate candidate;
		if (!parse_tile_base_name(base_name, candidate)) {
			continue;
		}
		seen_tiles.emplace(base_name, true);
		candidates.push_back(candidate);
	}
}

bool ArchiveReader::parse_tile_base_name(const std::string &p_base_name, TileCandidate &r_candidate) const {
	if (p_base_name.rfind("Tile_", 0) != 0 && p_base_name.rfind("QuadTree_", 0) != 0) {
		return false;
	}

	const size_t first_underscore = p_base_name.find('_');
	if (first_underscore == std::string::npos) {
		return false;
	}

	const std::string without_prefix = p_base_name.substr(first_underscore + 1);
	const size_t lat_end = without_prefix.find('_');
	if (lat_end == std::string::npos) {
		return false;
	}
	const size_t lon_end = without_prefix.find("_face", lat_end + 1);
	if (lon_end == std::string::npos) {
		return false;
	}

	const std::string lat_token = without_prefix.substr(0, lat_end);
	const std::string lon_token = without_prefix.substr(lat_end + 1, lon_end - lat_end - 1);
	if (lat_token.size() < 2 || lon_token.size() < 2) {
		return false;
	}
	const char lat_sign = lat_token.back();
	const char lon_sign = lon_token.back();
	double lat = std::strtod(lat_token.substr(0, lat_token.size() - 1).c_str(), nullptr);
	double lon = std::strtod(lon_token.substr(0, lon_token.size() - 1).c_str(), nullptr);

	if (lat_sign == 'S') {
		lat = -lat;
	} else if (lat_sign != 'N') {
		return false;
	}

	if (lon_sign == 'W') {
		lon = -lon;
	} else if (lon_sign != 'E') {
		return false;
	}

	const std::string face_and_sectors = without_prefix.substr(lon_end + 5);
	const size_t sectors_separator = face_and_sectors.find('_');
	if (sectors_separator == std::string::npos || sectors_separator == 0 || sectors_separator + 1 >= face_and_sectors.size()) {
		return false;
	}

	const int face = static_cast<int>(std::strtol(face_and_sectors.substr(0, sectors_separator).c_str(), nullptr, 10));
	const std::string sectors = face_and_sectors.substr(sectors_separator + 1);
	for (const char sector : sectors) {
		if (sector < '0' || sector > '3') {
			return false;
		}
	}

	r_candidate.base_name = p_base_name;
	r_candidate.latitude = lat;
	r_candidate.longitude = lon;
	r_candidate.face = face;
	r_candidate.depth = static_cast<int>(sectors.size()) - 1;
	return true;
}

bool ArchiveReader::read_hot(const std::string &p_base_name, TileData &r_tile) {
	std::vector<uint8_t> data;
	if (!read_file(build_hot_name(p_base_name), data)) {
		return false;
	}

	const uint8_t *cursor = data.data();
	size_t remaining = data.size();
	uint32_t points_used[TILE_LOD_COUNT] = {};
	int32_t indices_used[TILE_LOD_COUNT] = {};
	int32_t triangles_used[TILE_LOD_COUNT] = {};

	if (!read_value(cursor, remaining, r_tile.min_bbox) ||
			!read_value(cursor, remaining, r_tile.max_bbox) ||
			!read_array(cursor, remaining, points_used, TILE_LOD_COUNT) ||
			!read_array(cursor, remaining, indices_used, TILE_LOD_COUNT) ||
			!read_array(cursor, remaining, triangles_used, TILE_LOD_COUNT)) {
		error_text = "Invalid HOT data: " + p_base_name;
		return false;
	}

	return true;
}

bool ArchiveReader::read_dem(const std::string &p_base_name, int p_lod, uint32_t p_max_vertices, TileData &r_tile) {
	std::vector<uint8_t> data;
	if (!read_file(build_dem_name(p_base_name, p_lod), data)) {
		return false;
	}

	const uint8_t *cursor = data.data();
	size_t remaining = data.size();
	uint32_t point_count = 0;
	if (!read_value(cursor, remaining, point_count)) {
		error_text = "Invalid DEM header: " + p_base_name;
		return false;
	}

	if (point_count > p_max_vertices) {
		error_text = "Tile has too many vertices: " + p_base_name;
		return false;
	}

	r_tile.dem_points.resize(point_count);
	if (!read_array(cursor, remaining, r_tile.dem_points.data(), point_count)) {
		error_text = "Invalid DEM data: " + p_base_name;
		return false;
	}

	return true;
}

bool ArchiveReader::read_index(int p_lod, uint32_t p_vertex_count, TileData &r_tile) {
	std::vector<uint8_t> data;
	const std::string index_name = build_index_name(p_lod);
	if (!read_file(index_name, data)) {
		if (!read_external_file(index_name, data)) {
			return false;
		}
	}

	const uint8_t *cursor = data.data();
	size_t remaining = data.size();
	uint32_t index_count = 0;
	int32_t triangle_count = 0;
	if (!read_value(cursor, remaining, index_count) || !read_value(cursor, remaining, triangle_count)) {
		error_text = "Invalid index header.";
		return false;
	}

	if (index_count == 0 || triangle_count < 0) {
		error_text = "Invalid index count.";
		return false;
	}

	std::vector<uint32_t> strip_indices(index_count);
	if (!read_array(cursor, remaining, strip_indices.data(), index_count)) {
		error_text = "Invalid index data.";
		return false;
	}

	constexpr uint32_t restart_index = std::numeric_limits<uint32_t>::max();
	r_tile.indices.clear();
	r_tile.indices.reserve(static_cast<size_t>(std::max(0, triangle_count)) * 3);

	uint32_t previous0 = 0;
	uint32_t previous1 = 0;
	int window_count = 0;
	bool flip_winding = false;
	for (const uint32_t index : strip_indices) {
		if (index == restart_index) {
			window_count = 0;
			flip_winding = false;
			continue;
		}
		if (index >= p_vertex_count) {
			error_text = "Index points outside DEM vertex data.";
			return false;
		}

		if (window_count < 2) {
			if (window_count == 0) {
				previous0 = index;
			} else {
				previous1 = index;
			}
			window_count++;
			continue;
		}

		if (previous0 != previous1 && previous1 != index && previous0 != index) {
			if (flip_winding) {
				r_tile.indices.push_back(previous1);
				r_tile.indices.push_back(previous0);
				r_tile.indices.push_back(index);
			} else {
				r_tile.indices.push_back(previous0);
				r_tile.indices.push_back(previous1);
				r_tile.indices.push_back(index);
			}
		}
		previous0 = previous1;
		previous1 = index;
		flip_winding = !flip_winding;
	}

	return true;
}

bool ArchiveReader::read_texture(const std::string &p_base_name, int p_lod, TileData &r_tile) {
	std::vector<uint8_t> data;
	if (!read_file(build_texture_name(p_base_name, p_lod), data)) {
		return false;
	}

	r_tile.texture_data = std::move(data);
	r_tile.has_texture = !r_tile.texture_data.empty();
	return r_tile.has_texture;
}

} // namespace px_archive_terrain
