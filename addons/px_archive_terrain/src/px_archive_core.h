#pragma once

#include "../thirdparty/lzma/LzmaEnc.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace px_archive_terrain {

struct Vec3f {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct Vec3d {
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct TileDemPoint {
	Vec3f vertex;
	Vec3f normal;
	float uv[2] = {};
};

struct TileCandidate {
	static constexpr int MAX_DEPTH = 15;

	std::string base_name;
	double latitude = 0.0;
	double longitude = 0.0;
	int face = -1;
	int depth = -1;
	std::array<int, MAX_DEPTH> sectors = {};
};

struct TileData {
	std::string base_name;
	TileCandidate candidate;
	Vec3d min_bbox;
	Vec3d max_bbox;
	std::vector<TileDemPoint> dem_points;
	std::vector<uint32_t> indices;
	std::vector<uint8_t> texture_data;
	bool has_texture = false;
};

struct ArchivePathSet {
	std::string lat_path;
	std::string tile_path;
	std::string archive_path;
};

class ArchiveReader {
public:
	static constexpr int TILE_LOD_COUNT = 4;

	bool open(const std::string &p_archive_path);
	void close();

	bool is_open() const;
	const std::string &get_archive_path() const;
	const std::string &get_error_text() const;
	const std::vector<TileCandidate> &get_candidates() const;

	bool read_tile(const TileCandidate &p_candidate, int p_lod, int p_texture_lod, uint32_t p_max_vertices, TileData &r_tile, bool p_read_texture = true);

	static bool build_archive_paths(const std::string &p_terrain_folder, double p_latitude, double p_longitude, ArchivePathSet &r_paths);
	static std::vector<std::string> build_neighbor_archive_paths(const std::string &p_terrain_folder, double p_latitude, double p_longitude, int p_neighborhood);

private:
	struct RawFileHeader {
		char filename[64] = {};
		int64_t offset = 0;
		int64_t compressed_size = 0;
		int64_t uncompressed_size = 0;
		CLzmaEncProps props = {};
	};

	std::string archive_path;
	std::string terrain_root_path;
	std::string error_text;
	std::vector<RawFileHeader> headers;
	std::unordered_map<std::string, size_t> file_indices;
	std::vector<TileCandidate> candidates;

	bool read_file(const std::string &p_name, std::vector<uint8_t> &r_data);
	bool read_external_file(const std::string &p_name, std::vector<uint8_t> &r_data);
	bool find_file(const std::string &p_name, size_t &r_index) const;
	void build_candidates();
	bool parse_tile_base_name(const std::string &p_base_name, TileCandidate &r_candidate) const;
	bool read_hot(const std::string &p_base_name, TileData &r_tile);
	bool read_dem(const std::string &p_base_name, int p_lod, uint32_t p_max_vertices, TileData &r_tile);
	bool read_index(int p_lod, uint32_t p_vertex_count, TileData &r_tile);
	bool read_texture(const std::string &p_base_name, int p_lod, TileData &r_tile);
};

} // namespace px_archive_terrain
