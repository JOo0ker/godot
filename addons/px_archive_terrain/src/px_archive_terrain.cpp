#include "px_archive_terrain.h"

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace godot {
namespace {

Vector3 to_godot_vector(const px_archive_terrain::Vec3d &p_value) {
	return Vector3(static_cast<real_t>(p_value.x), static_cast<real_t>(p_value.y), static_cast<real_t>(p_value.z));
}

std::string tile_key_std(const std::string &p_archive_path, const std::string &p_base_name) {
	return p_archive_path + "|" + p_base_name;
}

} // namespace

bool PXArchiveTerrain::_prepare_tile_display_data(const px_archive_terrain::TileData &p_tile, const px_archive_terrain::GeoReference &p_geo_reference, TileDisplayData &r_display, std::string &r_error_text) {
	r_display = PXArchiveTerrain::TileDisplayData();
	r_display.base_name = p_tile.base_name;
	if (p_tile.has_texture && !p_tile.texture_data.empty()) {
		if (p_tile.texture_data.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
			r_error_text = "Tile texture is too large.";
			return true;
		}

		PackedByteArray texture_bytes;
		texture_bytes.resize(static_cast<int>(p_tile.texture_data.size()));
		for (int i = 0; i < static_cast<int>(p_tile.texture_data.size()); i++) {
			texture_bytes.set(i, p_tile.texture_data[static_cast<size_t>(i)]);
		}

		Ref<Image> image;
		image.instantiate();
		const Error texture_error = image->load_dds_from_buffer(texture_bytes);
		if (texture_error == OK && !image->is_empty()) {
			r_display.texture_image = image;
			r_display.has_texture = true;
		} else {
			r_error_text = "Failed to decode tile texture.";
		}
	}

	if (p_tile.dem_points.empty()) {
		const px_archive_terrain::Vec3d center = p_geo_reference.geo_to_local(p_tile.candidate.latitude, p_tile.candidate.longitude, 0.0);
		r_display.aabb = { center, { 0.0, 0.0, 0.0 } };
		return true;
	}

	const size_t vertex_count = p_tile.dem_points.size();
	r_display.vertices.resize(vertex_count * 3);
	r_display.normals.resize(vertex_count * 3);
	r_display.uvs.resize(vertex_count * 2);

	px_archive_terrain::Vec3d min_point;
	px_archive_terrain::Vec3d max_point;
	for (size_t i = 0; i < vertex_count; i++) {
		const px_archive_terrain::TileDemPoint &point = p_tile.dem_points[i];
		px_archive_terrain::Vec3f scaled_vertex = point.vertex;

		const px_archive_terrain::Vec3d local_vertex = p_geo_reference.source_tile_vertex_to_local(p_tile.candidate, scaled_vertex);
		r_display.vertices[(i * 3) + 0] = static_cast<float>(local_vertex.x);
		r_display.vertices[(i * 3) + 1] = static_cast<float>(local_vertex.y);
		r_display.vertices[(i * 3) + 2] = static_cast<float>(local_vertex.z);

		const Vector3 normal = Vector3(
				static_cast<real_t>(point.normal.x),
				static_cast<real_t>(point.normal.z),
				static_cast<real_t>(point.normal.y))
									   .normalized();
		r_display.normals[(i * 3) + 0] = static_cast<float>(normal.x);
		r_display.normals[(i * 3) + 1] = static_cast<float>(normal.y);
		r_display.normals[(i * 3) + 2] = static_cast<float>(normal.z);

		r_display.uvs[(i * 2) + 0] = point.uv[0];
		r_display.uvs[(i * 2) + 1] = point.uv[1];

		if (i == 0) {
			min_point = local_vertex;
			max_point = local_vertex;
		} else {
			min_point.x = std::min(min_point.x, local_vertex.x);
			min_point.y = std::min(min_point.y, local_vertex.y);
			min_point.z = std::min(min_point.z, local_vertex.z);
			max_point.x = std::max(max_point.x, local_vertex.x);
			max_point.y = std::max(max_point.y, local_vertex.y);
			max_point.z = std::max(max_point.z, local_vertex.z);
		}
	}

	r_display.indices.resize(p_tile.indices.size());
	for (size_t i = 0; i < p_tile.indices.size(); i++) {
		if (p_tile.indices[i] > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
			r_error_text = "Tile index is too large.";
			return false;
		}
		r_display.indices[i] = static_cast<int32_t>(p_tile.indices[i]);
	}

	r_display.aabb = {
		min_point,
		{ max_point.x - min_point.x, max_point.y - min_point.y, max_point.z - min_point.z },
	};
	return true;
}

std::string PXArchiveTerrain::to_utf8(const String &p_value) {
	return std::string(p_value.utf8().get_data());
}

String PXArchiveTerrain::from_utf8(const std::string &p_value) {
	return String::utf8(p_value.c_str());
}

void PXArchiveTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_terrain_folder", "path"), &PXArchiveTerrain::set_terrain_folder);
	ClassDB::bind_method(D_METHOD("get_terrain_folder"), &PXArchiveTerrain::get_terrain_folder);
	ClassDB::bind_method(D_METHOD("set_origin_latitude", "latitude"), &PXArchiveTerrain::set_origin_latitude);
	ClassDB::bind_method(D_METHOD("get_origin_latitude"), &PXArchiveTerrain::get_origin_latitude);
	ClassDB::bind_method(D_METHOD("set_origin_longitude", "longitude"), &PXArchiveTerrain::set_origin_longitude);
	ClassDB::bind_method(D_METHOD("get_origin_longitude"), &PXArchiveTerrain::get_origin_longitude);
	ClassDB::bind_method(D_METHOD("set_origin_altitude", "altitude"), &PXArchiveTerrain::set_origin_altitude);
	ClassDB::bind_method(D_METHOD("get_origin_altitude"), &PXArchiveTerrain::get_origin_altitude);
	ClassDB::bind_method(D_METHOD("set_origin", "latitude", "longitude", "altitude"), &PXArchiveTerrain::set_origin);
	ClassDB::bind_method(D_METHOD("set_editor_eye_global_position", "global_position"), &PXArchiveTerrain::set_editor_eye_global_position);
	ClassDB::bind_method(D_METHOD("set_editor_camera_state", "global_position", "frustum"), &PXArchiveTerrain::set_editor_camera_state);
	ClassDB::bind_method(D_METHOD("clear_editor_eye_override"), &PXArchiveTerrain::clear_editor_eye_override);
	ClassDB::bind_method(D_METHOD("set_lod", "lod"), &PXArchiveTerrain::set_lod);
	ClassDB::bind_method(D_METHOD("get_lod"), &PXArchiveTerrain::get_lod);
	ClassDB::bind_method(D_METHOD("set_texture_lod", "lod"), &PXArchiveTerrain::set_texture_lod);
	ClassDB::bind_method(D_METHOD("get_texture_lod"), &PXArchiveTerrain::get_texture_lod);
	ClassDB::bind_method(D_METHOD("set_max_tiles_per_update", "max_tiles"), &PXArchiveTerrain::set_max_tiles_per_update);
	ClassDB::bind_method(D_METHOD("get_max_tiles_per_update"), &PXArchiveTerrain::get_max_tiles_per_update);
	ClassDB::bind_method(D_METHOD("set_max_tile_jobs_per_refresh", "max_tiles"), &PXArchiveTerrain::set_max_tile_jobs_per_refresh);
	ClassDB::bind_method(D_METHOD("get_max_tile_jobs_per_refresh"), &PXArchiveTerrain::get_max_tile_jobs_per_refresh);
	ClassDB::bind_method(D_METHOD("set_max_tile_attach_per_frame", "max_tiles"), &PXArchiveTerrain::set_max_tile_attach_per_frame);
	ClassDB::bind_method(D_METHOD("get_max_tile_attach_per_frame"), &PXArchiveTerrain::get_max_tile_attach_per_frame);
	ClassDB::bind_method(D_METHOD("set_max_vertices_per_tile", "max_vertices"), &PXArchiveTerrain::set_max_vertices_per_tile);
	ClassDB::bind_method(D_METHOD("get_max_vertices_per_tile"), &PXArchiveTerrain::get_max_vertices_per_tile);
	ClassDB::bind_method(D_METHOD("set_archive_neighborhood", "neighborhood"), &PXArchiveTerrain::set_archive_neighborhood);
	ClassDB::bind_method(D_METHOD("get_archive_neighborhood"), &PXArchiveTerrain::get_archive_neighborhood);
	ClassDB::bind_method(D_METHOD("set_vertical_range", "range"), &PXArchiveTerrain::set_vertical_range);
	ClassDB::bind_method(D_METHOD("get_vertical_range"), &PXArchiveTerrain::get_vertical_range);
	ClassDB::bind_method(D_METHOD("set_loading_range", "range"), &PXArchiveTerrain::set_loading_range);
	ClassDB::bind_method(D_METHOD("get_loading_range"), &PXArchiveTerrain::get_loading_range);
	ClassDB::bind_method(D_METHOD("set_cleanup_range_multiplier", "multiplier"), &PXArchiveTerrain::set_cleanup_range_multiplier);
	ClassDB::bind_method(D_METHOD("get_cleanup_range_multiplier"), &PXArchiveTerrain::get_cleanup_range_multiplier);
	ClassDB::bind_method(D_METHOD("set_max_tile_attach_time_ms", "milliseconds"), &PXArchiveTerrain::set_max_tile_attach_time_ms);
	ClassDB::bind_method(D_METHOD("get_max_tile_attach_time_ms"), &PXArchiveTerrain::get_max_tile_attach_time_ms);
	ClassDB::bind_method(D_METHOD("set_auto_refresh_interval", "interval"), &PXArchiveTerrain::set_auto_refresh_interval);
	ClassDB::bind_method(D_METHOD("get_auto_refresh_interval"), &PXArchiveTerrain::get_auto_refresh_interval);
	ClassDB::bind_method(D_METHOD("set_auto_refresh_min_distance", "distance"), &PXArchiveTerrain::set_auto_refresh_min_distance);
	ClassDB::bind_method(D_METHOD("get_auto_refresh_min_distance"), &PXArchiveTerrain::get_auto_refresh_min_distance);
	ClassDB::bind_method(D_METHOD("set_auto_refresh_min_angle", "degrees"), &PXArchiveTerrain::set_auto_refresh_min_angle);
	ClassDB::bind_method(D_METHOD("get_auto_refresh_min_angle"), &PXArchiveTerrain::get_auto_refresh_min_angle);
	ClassDB::bind_method(D_METHOD("set_worker_thread_count", "count"), &PXArchiveTerrain::set_worker_thread_count);
	ClassDB::bind_method(D_METHOD("get_worker_thread_count"), &PXArchiveTerrain::get_worker_thread_count);
	ClassDB::bind_method(D_METHOD("refresh_visible_tiles"), &PXArchiveTerrain::refresh_visible_tiles);
	ClassDB::bind_method(D_METHOD("clear_tiles"), &PXArchiveTerrain::clear_tiles);
	ClassDB::bind_method(D_METHOD("get_loaded_archive_count"), &PXArchiveTerrain::get_loaded_archive_count);
	ClassDB::bind_method(D_METHOD("get_loaded_tile_count"), &PXArchiveTerrain::get_loaded_tile_count);
	ClassDB::bind_method(D_METHOD("get_last_error"), &PXArchiveTerrain::get_last_error);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "terrain_folder", PROPERTY_HINT_DIR), "set_terrain_folder", "get_terrain_folder");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "origin_latitude"), "set_origin_latitude", "get_origin_latitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "origin_longitude"), "set_origin_longitude", "get_origin_longitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "origin_altitude"), "set_origin_altitude", "get_origin_altitude");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod", PROPERTY_HINT_RANGE, "0,3,1"), "set_lod", "get_lod");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_lod", PROPERTY_HINT_RANGE, "0,3,1"), "set_texture_lod", "get_texture_lod");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tiles_per_update", PROPERTY_HINT_RANGE, "1,4096,1"), "set_max_tiles_per_update", "get_max_tiles_per_update");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tile_jobs_per_refresh", PROPERTY_HINT_RANGE, "1,4096,1"), "set_max_tile_jobs_per_refresh", "get_max_tile_jobs_per_refresh");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tile_attach_per_frame", PROPERTY_HINT_RANGE, "1,512,1"), "set_max_tile_attach_per_frame", "get_max_tile_attach_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_vertices_per_tile", PROPERTY_HINT_RANGE, "3,2000000,1"), "set_max_vertices_per_tile", "get_max_vertices_per_tile");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "archive_neighborhood", PROPERTY_HINT_RANGE, "0,4,1"), "set_archive_neighborhood", "get_archive_neighborhood");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vertical_range"), "set_vertical_range", "get_vertical_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loading_range", PROPERTY_HINT_RANGE, "1000,1000000,1000"), "set_loading_range", "get_loading_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cleanup_range_multiplier", PROPERTY_HINT_RANGE, "1,4,0.05"), "set_cleanup_range_multiplier", "get_cleanup_range_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_tile_attach_time_ms", PROPERTY_HINT_RANGE, "0.1,20,0.1"), "set_max_tile_attach_time_ms", "get_max_tile_attach_time_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_refresh_interval", PROPERTY_HINT_RANGE, "0.05,10,0.05"), "set_auto_refresh_interval", "get_auto_refresh_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_refresh_min_distance", PROPERTY_HINT_RANGE, "0,100000,100"), "set_auto_refresh_min_distance", "get_auto_refresh_min_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_refresh_min_angle", PROPERTY_HINT_RANGE, "0,180,0.1"), "set_auto_refresh_min_angle", "get_auto_refresh_min_angle");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "worker_thread_count", PROPERTY_HINT_RANGE, "1,8,1"), "set_worker_thread_count", "get_worker_thread_count");
}

void PXArchiveTerrain::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		_start_workers();
		set_process(true);
	} else if (p_what == NOTIFICATION_PROCESS) {
		_drain_completed_tiles();
		auto_refresh_elapsed += get_process_delta_time();
		if (auto_refresh_elapsed >= auto_refresh_interval) {
			auto_refresh_elapsed = 0.0;
			_refresh_from_current_eye(true);
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		set_process(false);
		has_editor_eye_override = false;
		has_editor_camera_frustum = false;
		_stop_workers();
	}
}

void PXArchiveTerrain::set_terrain_folder(const String &p_path) {
	terrain_folder = p_path;
	loaded_archives.clear();
	_clear_pending_loads();
	_request_auto_refresh();
	_clear_tiles();
}

String PXArchiveTerrain::get_terrain_folder() const {
	return terrain_folder;
}

void PXArchiveTerrain::set_origin_latitude(double p_value) {
	set_origin(p_value, geo_reference.get_origin_longitude(), geo_reference.get_origin_altitude());
}

double PXArchiveTerrain::get_origin_latitude() const {
	return geo_reference.get_origin_latitude();
}

void PXArchiveTerrain::set_origin_longitude(double p_value) {
	set_origin(geo_reference.get_origin_latitude(), p_value, geo_reference.get_origin_altitude());
}

double PXArchiveTerrain::get_origin_longitude() const {
	return geo_reference.get_origin_longitude();
}

void PXArchiveTerrain::set_origin_altitude(double p_value) {
	set_origin(geo_reference.get_origin_latitude(), geo_reference.get_origin_longitude(), p_value);
}

double PXArchiveTerrain::get_origin_altitude() const {
	return geo_reference.get_origin_altitude();
}

void PXArchiveTerrain::set_origin(double p_latitude, double p_longitude, double p_altitude) {
	geo_reference.set_origin(p_latitude, p_longitude, p_altitude);
	_clear_pending_loads();
	_request_auto_refresh();
	_clear_tiles();
}

void PXArchiveTerrain::set_editor_eye_global_position(const Vector3 &p_global_position) {
	editor_eye_global_position = p_global_position;
	has_editor_eye_override = true;
	auto_refresh_elapsed = auto_refresh_interval;
	_request_auto_refresh();
	_refresh_from_current_eye(true);
}

void PXArchiveTerrain::set_editor_camera_state(const Vector3 &p_global_position, const Array &p_frustum) {
	editor_eye_global_position = p_global_position;
	editor_camera_frustum = p_frustum;
	has_editor_eye_override = true;
	has_editor_camera_frustum = true;
	auto_refresh_elapsed = auto_refresh_interval;
	_request_auto_refresh();
	_refresh_from_current_eye(true);
}

void PXArchiveTerrain::clear_editor_eye_override() {
	has_editor_eye_override = false;
	has_editor_camera_frustum = false;
	editor_camera_frustum.clear();
}

void PXArchiveTerrain::set_lod(int p_lod) {
	lod = std::clamp(p_lod, 0, px_archive_terrain::ArchiveReader::TILE_LOD_COUNT - 1);
	_force_refresh_after_tiles_changed();
}

int PXArchiveTerrain::get_lod() const {
	return lod;
}

void PXArchiveTerrain::set_texture_lod(int p_lod) {
	texture_lod = std::clamp(p_lod, 0, px_archive_terrain::ArchiveReader::TILE_LOD_COUNT - 1);
	_force_refresh_after_tiles_changed();
}

int PXArchiveTerrain::get_texture_lod() const {
	return texture_lod;
}

void PXArchiveTerrain::set_max_tiles_per_update(int p_max_tiles) {
	max_tiles_per_update = std::max(1, p_max_tiles);
	max_tile_jobs_per_refresh = max_tiles_per_update;
	max_tile_attach_per_frame = std::min(max_tile_attach_per_frame, max_tiles_per_update);
	_request_auto_refresh();
}

int PXArchiveTerrain::get_max_tiles_per_update() const {
	return max_tiles_per_update;
}

void PXArchiveTerrain::set_max_tile_jobs_per_refresh(int p_max_tiles) {
	max_tile_jobs_per_refresh = std::max(1, p_max_tiles);
	max_tiles_per_update = max_tile_jobs_per_refresh;
	_request_auto_refresh();
}

int PXArchiveTerrain::get_max_tile_jobs_per_refresh() const {
	return max_tile_jobs_per_refresh;
}

void PXArchiveTerrain::set_max_tile_attach_per_frame(int p_max_tiles) {
	max_tile_attach_per_frame = std::max(1, p_max_tiles);
}

int PXArchiveTerrain::get_max_tile_attach_per_frame() const {
	return max_tile_attach_per_frame;
}

void PXArchiveTerrain::set_max_vertices_per_tile(int p_max_vertices) {
	max_vertices_per_tile = std::max(3, p_max_vertices);
}

int PXArchiveTerrain::get_max_vertices_per_tile() const {
	return max_vertices_per_tile;
}

void PXArchiveTerrain::set_archive_neighborhood(int p_neighborhood) {
	archive_neighborhood = std::max(0, p_neighborhood);
	_clear_pending_loads();
	_request_auto_refresh();
}

int PXArchiveTerrain::get_archive_neighborhood() const {
	return archive_neighborhood;
}

void PXArchiveTerrain::set_vertical_range(double p_range) {
	vertical_range = std::max(1.0, p_range);
}

double PXArchiveTerrain::get_vertical_range() const {
	return vertical_range;
}

void PXArchiveTerrain::set_loading_range(double p_range) {
	loading_range = std::max(1000.0, p_range);
	_request_auto_refresh();
}

double PXArchiveTerrain::get_loading_range() const {
	return loading_range;
}

void PXArchiveTerrain::set_cleanup_range_multiplier(double p_multiplier) {
	cleanup_range_multiplier = std::max(1.0, p_multiplier);
}

double PXArchiveTerrain::get_cleanup_range_multiplier() const {
	return cleanup_range_multiplier;
}

void PXArchiveTerrain::set_max_tile_attach_time_ms(double p_milliseconds) {
	max_tile_attach_time_ms = std::max(0.1, p_milliseconds);
}

double PXArchiveTerrain::get_max_tile_attach_time_ms() const {
	return max_tile_attach_time_ms;
}

void PXArchiveTerrain::set_auto_refresh_interval(double p_interval) {
	auto_refresh_interval = std::max(0.05, p_interval);
}

double PXArchiveTerrain::get_auto_refresh_interval() const {
	return auto_refresh_interval;
}

void PXArchiveTerrain::set_auto_refresh_min_distance(double p_distance) {
	auto_refresh_min_distance = std::max(0.0, p_distance);
}

double PXArchiveTerrain::get_auto_refresh_min_distance() const {
	return auto_refresh_min_distance;
}

void PXArchiveTerrain::set_auto_refresh_min_angle(double p_degrees) {
	auto_refresh_min_angle = std::clamp(p_degrees, 0.0, 180.0);
}

double PXArchiveTerrain::get_auto_refresh_min_angle() const {
	return auto_refresh_min_angle;
}

void PXArchiveTerrain::set_worker_thread_count(int p_count) {
	const int clamped_count = std::clamp(p_count, 1, 8);
	if (worker_thread_count == clamped_count) {
		return;
	}

	const bool restart_workers = is_inside_tree() && !workers.empty();
	if (restart_workers) {
		_stop_workers();
	}
	worker_thread_count = clamped_count;
	if (restart_workers) {
		_start_workers();
		_request_auto_refresh();
	}
}

int PXArchiveTerrain::get_worker_thread_count() const {
	return worker_thread_count;
}

void PXArchiveTerrain::refresh_visible_tiles() {
	_load_visible_tiles();
}

void PXArchiveTerrain::clear_tiles() {
	_clear_pending_loads();
	_clear_tiles();
}

int PXArchiveTerrain::get_loaded_archive_count() const {
	return static_cast<int>(loaded_archives.size());
}

int PXArchiveTerrain::get_loaded_tile_count() const {
	return static_cast<int>(loaded_tiles.size());
}

String PXArchiveTerrain::get_last_error() const {
	return last_error;
}

void PXArchiveTerrain::_clear_tiles() {
	for (auto &entry : loaded_tiles) {
		if (entry.second.node) {
			entry.second.node->queue_free();
		}
	}
	loaded_tiles.clear();
	queued_tiles.clear();
	failed_tiles.clear();
}

void PXArchiveTerrain::_remove_tile(const std::string &p_key) {
	const auto found = loaded_tiles.find(p_key);
	if (found == loaded_tiles.end()) {
		return;
	}

	MeshInstance3D *node = found->second.node;
	if (node) {
		node->queue_free();
	}
	loaded_tiles.erase(found);
}

void PXArchiveTerrain::_refresh_archives_for_eye(double p_latitude, double p_longitude) {
	const std::vector<std::string> desired_paths = px_archive_terrain::ArchiveReader::build_neighbor_archive_paths(to_utf8(terrain_folder), p_latitude, p_longitude, archive_neighborhood);
	std::unordered_set<std::string> desired_set(desired_paths.begin(), desired_paths.end());

	bool removing_archive = false;
	for (const auto &entry : loaded_archives) {
		if (desired_set.find(entry.first) == desired_set.end()) {
			removing_archive = true;
			break;
		}
	}
	if (removing_archive) {
		_clear_pending_loads();
	}

	for (auto it = loaded_archives.begin(); it != loaded_archives.end();) {
		if (desired_set.find(it->first) == desired_set.end()) {
			for (auto tile_it = loaded_tiles.begin(); tile_it != loaded_tiles.end();) {
				if (tile_it->second.archive_path == it->first) {
					if (tile_it->second.node) {
						tile_it->second.node->queue_free();
					}
					tile_it = loaded_tiles.erase(tile_it);
				} else {
					++tile_it;
				}
			}
			it = loaded_archives.erase(it);
		} else {
			++it;
		}
	}

	for (const std::string &archive_path : desired_paths) {
		if (loaded_archives.find(archive_path) != loaded_archives.end()) {
			continue;
		}

		auto reader = std::make_shared<px_archive_terrain::ArchiveReader>();
		if (!reader->open(archive_path)) {
			last_error = from_utf8(reader->get_error_text());
			continue;
		}

		ArchiveState state;
		state.reader = std::move(reader);
		loaded_archives.emplace(archive_path, std::move(state));
	}
}

bool PXArchiveTerrain::_get_eye_geodetic(double &r_latitude, double &r_longitude, double &r_altitude) const {
	if (Engine::get_singleton()->is_editor_hint() && has_editor_eye_override) {
		const Vector3 local_eye = get_global_transform().affine_inverse().xform(editor_eye_global_position);
		const px_archive_terrain::Vec3d local_eye_geo_input{ local_eye.x, local_eye.y, local_eye.z };
		geo_reference.local_to_geo(local_eye_geo_input, r_latitude, r_longitude, r_altitude);
		return true;
	}

	const Viewport *viewport = get_viewport();
	if (!viewport) {
		return false;
	}

	Camera3D *camera = viewport->get_camera_3d();
	if (!camera) {
		return false;
	}

	const Vector3 local_eye = get_global_transform().affine_inverse().xform(camera->get_global_position());
	const px_archive_terrain::Vec3d local_eye_geo_input{ local_eye.x, local_eye.y, local_eye.z };
	geo_reference.local_to_geo(local_eye_geo_input, r_latitude, r_longitude, r_altitude);
	return true;
}

bool PXArchiveTerrain::_get_camera_basis(Vector3 &r_forward, Vector3 &r_up) const {
	const Viewport *viewport = get_viewport();
	if (!viewport) {
		return false;
	}

	Camera3D *camera = viewport->get_camera_3d();
	if (!camera) {
		return false;
	}

	const Basis basis = camera->get_global_transform().basis;
	r_forward = (-basis.get_column(2)).normalized();
	r_up = basis.get_column(1).normalized();
	return true;
}

bool PXArchiveTerrain::_should_auto_refresh(double p_latitude, double p_longitude, double p_altitude) const {
	if (!has_last_refresh_eye) {
		return true;
	}

	Vector3 current_forward;
	Vector3 current_up;
	if (_get_camera_basis(current_forward, current_up)) {
		if (!has_last_refresh_camera_basis) {
			return true;
		}
		const double forward_dot = std::clamp(static_cast<double>(last_refresh_camera_forward.dot(current_forward)), -1.0, 1.0);
		const double up_dot = std::clamp(static_cast<double>(last_refresh_camera_up.dot(current_up)), -1.0, 1.0);
		const double forward_angle = std::acos(forward_dot) / px_archive_terrain::GeoReference::DEG_TO_RAD;
		const double up_angle = std::acos(up_dot) / px_archive_terrain::GeoReference::DEG_TO_RAD;
		if (std::max(forward_angle, up_angle) >= auto_refresh_min_angle) {
			return true;
		}
	}

	const px_archive_terrain::Vec3d previous = geo_reference.geo_to_local(last_refresh_latitude, last_refresh_longitude, last_refresh_altitude);
	const px_archive_terrain::Vec3d current = geo_reference.geo_to_local(p_latitude, p_longitude, p_altitude);
	const double dx = current.x - previous.x;
	const double dy = current.y - previous.y;
	const double dz = current.z - previous.z;
	const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
	return distance >= auto_refresh_min_distance;
}

void PXArchiveTerrain::_remember_refresh_eye(double p_latitude, double p_longitude, double p_altitude) {
	last_refresh_latitude = p_latitude;
	last_refresh_longitude = p_longitude;
	last_refresh_altitude = p_altitude;
	Vector3 current_forward;
	Vector3 current_up;
	if (_get_camera_basis(current_forward, current_up)) {
		last_refresh_camera_forward = current_forward;
		last_refresh_camera_up = current_up;
		has_last_refresh_camera_basis = true;
	} else {
		has_last_refresh_camera_basis = false;
	}
	has_last_refresh_eye = true;
}

void PXArchiveTerrain::_force_refresh_after_tiles_changed() {
	_clear_pending_loads();
	_clear_tiles();
	has_last_refresh_eye = false;
	auto_refresh_elapsed = auto_refresh_interval;
}

void PXArchiveTerrain::_request_auto_refresh() {
	has_last_refresh_eye = false;
	has_last_refresh_camera_basis = false;
	auto_refresh_elapsed = auto_refresh_interval;
}

bool PXArchiveTerrain::_refresh_from_current_eye(bool p_respect_refresh_gate) {
	if (!is_inside_tree() || terrain_folder.is_empty()) {
		return false;
	}

	double latitude = 0.0;
	double longitude = 0.0;
	double altitude = 0.0;
	if (!_get_eye_geodetic(latitude, longitude, altitude)) {
		last_error = "No eye point is available.";
		return false;
	}

	const bool should_refresh = !p_respect_refresh_gate || _should_auto_refresh(latitude, longitude, altitude);
	if (!should_refresh) {
		if (!_has_pending_loads()) {
			_load_visible_tiles(latitude, longitude, altitude);
		}
		return false;
	}

	_load_visible_tiles(latitude, longitude, altitude);
	_remember_refresh_eye(latitude, longitude, altitude);
	return true;
}

void PXArchiveTerrain::_start_workers() {
	if (!workers.empty()) {
		return;
	}

	worker_stop = false;
	for (int i = 0; i < worker_thread_count; i++) {
		workers.emplace_back(&PXArchiveTerrain::_worker_loop, this);
	}
}

void PXArchiveTerrain::_stop_workers() {
	{
		std::lock_guard<std::mutex> lock(worker_mutex);
		worker_stop = true;
		pending_jobs.clear();
	}
	worker_cv.notify_all();

	for (std::thread &worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	workers.clear();

	std::lock_guard<std::mutex> lock(worker_mutex);
	completed_jobs.clear();
	queued_tiles.clear();
	worker_stop = false;
}

void PXArchiveTerrain::_worker_loop() {
	std::unordered_map<std::string, std::unique_ptr<px_archive_terrain::ArchiveReader>> reader_cache;
	while (true) {
		TileLoadJob job;
		{
			std::unique_lock<std::mutex> lock(worker_mutex);
			worker_cv.wait(lock, [this]() {
				return worker_stop || !pending_jobs.empty();
			});
			if (worker_stop && pending_jobs.empty()) {
				return;
			}
			job = std::move(pending_jobs.front());
			pending_jobs.pop_front();
		}

		TileLoadResult result;
		result.key = job.key;
		result.archive_path = job.archive_path;
		result.generation = job.generation;
		px_archive_terrain::ArchiveReader *reader = nullptr;
		auto found_reader = reader_cache.find(job.archive_path);
		if (found_reader != reader_cache.end()) {
			reader = found_reader->second.get();
		} else {
			auto new_reader = std::make_unique<px_archive_terrain::ArchiveReader>();
			if (new_reader->open(job.archive_path)) {
				reader = new_reader.get();
				reader_cache.emplace(job.archive_path, std::move(new_reader));
			} else {
				result.error_text = new_reader->get_error_text();
			}
		}
		if (reader) {
			px_archive_terrain::TileData tile;
			result.success = reader->read_tile(job.candidate, job.lod, job.texture_lod, job.max_vertices, tile, true);
			if (result.success) {
				px_archive_terrain::GeoReference job_geo_reference;
				job_geo_reference.set_origin(job.origin_latitude, job.origin_longitude, job.origin_altitude);
				result.success = _prepare_tile_display_data(tile, job_geo_reference, result.display, result.error_text);
			} else {
				result.error_text = reader->get_error_text();
			}
		}

		{
			std::lock_guard<std::mutex> lock(worker_mutex);
			completed_jobs.push_back(std::move(result));
		}
	}
}

void PXArchiveTerrain::_clear_pending_loads() {
	std::lock_guard<std::mutex> lock(worker_mutex);
	pending_jobs.clear();
	completed_jobs.clear();
	queued_tiles.clear();
	failed_tiles.clear();
	load_generation++;
}

void PXArchiveTerrain::_drain_completed_tiles() {
	const auto start_time = std::chrono::steady_clock::now();
	int attached_this_update = 0;
	while (attached_this_update < max_tile_attach_per_frame) {
		TileLoadResult result;
		{
			std::lock_guard<std::mutex> lock(worker_mutex);
			if (completed_jobs.empty()) {
				break;
			}
			result = std::move(completed_jobs.front());
			completed_jobs.pop_front();
		}

		queued_tiles.erase(result.key);
		if (result.generation != load_generation || loaded_archives.find(result.archive_path) == loaded_archives.end() || loaded_tiles.find(result.key) != loaded_tiles.end()) {
			continue;
		}
		if (!result.success) {
			failed_tiles.insert(result.key);
			last_error = from_utf8(result.error_text);
			continue;
		}
		if (!result.error_text.empty()) {
			last_error = from_utf8(result.error_text);
		}

		if (!_aabb_is_near_eye(result.display.aabb, last_refresh_latitude, last_refresh_longitude, last_refresh_altitude, cleanup_range_multiplier)) {
			continue;
		}

		MeshInstance3D *mesh_instance = _create_tile_mesh(result.display);
		add_child(mesh_instance);
		loaded_tiles.emplace(result.key, LoadedTile{ mesh_instance, result.display.aabb, result.archive_path });
		attached_this_update++;
		const double elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
		if (elapsed_ms >= max_tile_attach_time_ms) {
			break;
		}
	}
}

void PXArchiveTerrain::_queue_visible_tile_jobs(double p_latitude, double p_longitude, double p_altitude) {
	struct CandidateJob {
		double distance = 0.0;
		std::string key;
		std::string archive_path;
		px_archive_terrain::TileCandidate candidate;
	};

	const px_archive_terrain::Vec3d eye = geo_reference.geo_to_local(p_latitude, p_longitude, p_altitude);
	std::vector<CandidateJob> candidates;
	for (auto &archive_entry : loaded_archives) {
		const std::string &archive_path = archive_entry.first;
		ArchiveState &archive_state = archive_entry.second;
		if (!archive_state.reader) {
			continue;
		}

		for (const px_archive_terrain::TileCandidate &candidate : archive_state.reader->get_candidates()) {
			const std::string key = tile_key_std(archive_path, candidate.base_name);
			if (loaded_tiles.find(key) != loaded_tiles.end() || queued_tiles.find(key) != queued_tiles.end() || failed_tiles.find(key) != failed_tiles.end()) {
				continue;
			}

			const px_archive_terrain::Aabb estimate = geo_reference.estimate_tile_aabb(candidate, vertical_range);
			if (!_aabb_is_near_eye(estimate, p_latitude, p_longitude, p_altitude, 1.0)) {
				continue;
			}
			const bool visible = _aabb_intersects_camera(estimate);

			const px_archive_terrain::Vec3d center = geo_reference.geo_to_local(candidate.latitude, candidate.longitude, 0.0);
			const double dx = center.x - eye.x;
			const double dy = center.y - eye.y;
			const double dz = center.z - eye.z;
			const double priority_penalty = visible ? 0.0 : 1.0e18;
			candidates.push_back(CandidateJob{
					dx * dx + dy * dy + dz * dz + priority_penalty,
					key,
					archive_path,
					candidate });
		}
	}

	std::sort(candidates.begin(), candidates.end(), [](const CandidateJob &p_lhs, const CandidateJob &p_rhs) {
		return p_lhs.distance < p_rhs.distance;
	});

	int queued_this_update = 0;
	{
		std::lock_guard<std::mutex> lock(worker_mutex);
		for (const CandidateJob &candidate : candidates) {
			if (queued_this_update >= max_tile_jobs_per_refresh) {
				break;
			}
			if (queued_tiles.insert(candidate.key).second) {
				pending_jobs.push_back(TileLoadJob{
						candidate.key,
						candidate.archive_path,
						candidate.candidate,
						load_generation,
						lod,
						texture_lod,
						static_cast<uint32_t>(max_vertices_per_tile),
						geo_reference.get_origin_latitude(),
						geo_reference.get_origin_longitude(),
						geo_reference.get_origin_altitude() });
				queued_this_update++;
			}
		}
	}
	if (queued_this_update > 0) {
		worker_cv.notify_all();
	}
}

bool PXArchiveTerrain::_has_pending_loads() {
	std::lock_guard<std::mutex> lock(worker_mutex);
	return !pending_jobs.empty() || !completed_jobs.empty() || !queued_tiles.empty();
}

bool PXArchiveTerrain::_aabb_intersects_camera(const px_archive_terrain::Aabb &p_aabb) const {
	const AABB local_aabb(to_godot_vector(p_aabb.position), to_godot_vector(p_aabb.size));
	const AABB global_aabb = get_global_transform().xform(local_aabb);
	Array frustum;
	if (Engine::get_singleton()->is_editor_hint() && has_editor_camera_frustum) {
		frustum = editor_camera_frustum;
	} else {
		const Viewport *viewport = get_viewport();
		if (!viewport) {
			return false;
		}

		Camera3D *camera = viewport->get_camera_3d();
		if (!camera) {
			return false;
		}
		frustum = camera->get_frustum();
	}

	for (int i = 0; i < frustum.size(); i++) {
		const Plane plane = frustum[i];
		const Vector3 support = global_aabb.get_support(-plane.normal);
		if (plane.is_point_over(support)) {
			return false;
		}
	}

	return true;
}

bool PXArchiveTerrain::_aabb_is_near_eye(const px_archive_terrain::Aabb &p_aabb, double p_latitude, double p_longitude, double p_altitude, double p_multiplier) const {
	double max_distance = loading_range;
	max_distance *= std::max(0.1, p_multiplier);

	const px_archive_terrain::Vec3d eye = geo_reference.geo_to_local(p_latitude, p_longitude, p_altitude);
	const double min_x = p_aabb.position.x;
	const double min_y = p_aabb.position.y;
	const double min_z = p_aabb.position.z;
	const double max_x = p_aabb.position.x + p_aabb.size.x;
	const double max_y = p_aabb.position.y + p_aabb.size.y;
	const double max_z = p_aabb.position.z + p_aabb.size.z;
	const double closest_x = std::clamp(eye.x, std::min(min_x, max_x), std::max(min_x, max_x));
	const double closest_y = std::clamp(eye.y, std::min(min_y, max_y), std::max(min_y, max_y));
	const double closest_z = std::clamp(eye.z, std::min(min_z, max_z), std::max(min_z, max_z));
	const double dx = eye.x - closest_x;
	const double dy = eye.y - closest_y;
	const double dz = eye.z - closest_z;
	return dx * dx + dy * dy + dz * dz <= max_distance * max_distance;
}

MeshInstance3D *PXArchiveTerrain::_create_tile_mesh(const TileDisplayData &p_tile) {
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	const int vertex_count = static_cast<int>(p_tile.vertices.size() / 3);
	vertices.resize(vertex_count);
	normals.resize(vertex_count);
	uvs.resize(static_cast<int>(p_tile.uvs.size() / 2));
	for (int i = 0; i < vertex_count; i++) {
		vertices.set(i, Vector3(static_cast<real_t>(p_tile.vertices[(static_cast<size_t>(i) * 3) + 0]), static_cast<real_t>(p_tile.vertices[(static_cast<size_t>(i) * 3) + 1]), static_cast<real_t>(p_tile.vertices[(static_cast<size_t>(i) * 3) + 2])));
		normals.set(i, Vector3(static_cast<real_t>(p_tile.normals[(static_cast<size_t>(i) * 3) + 0]), static_cast<real_t>(p_tile.normals[(static_cast<size_t>(i) * 3) + 1]), static_cast<real_t>(p_tile.normals[(static_cast<size_t>(i) * 3) + 2])));
		uvs.set(i, Vector2(p_tile.uvs[(static_cast<size_t>(i) * 2) + 0], p_tile.uvs[(static_cast<size_t>(i) * 2) + 1]));
	}

	indices.resize(static_cast<int>(p_tile.indices.size()));
	for (int i = 0; i < static_cast<int>(p_tile.indices.size()); i++) {
		indices.set(i, p_tile.indices[static_cast<size_t>(i)]);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name(from_utf8(p_tile.base_name));
	mesh_instance->set_mesh(mesh);
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_albedo(Color(1.0, 1.0, 1.0, 1.0));
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	material->set_cull_mode(BaseMaterial3D::CULL_BACK);
	if (p_tile.has_texture && p_tile.texture_image.is_valid() && !p_tile.texture_image->is_empty()) {
		Ref<ImageTexture> texture = ImageTexture::create_from_image(p_tile.texture_image);
		material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
	}
	mesh_instance->set_surface_override_material(0, material);
	return mesh_instance;
}

void PXArchiveTerrain::_load_visible_tiles() {
	if (terrain_folder.is_empty()) {
		return;
	}

	double latitude = 0.0;
	double longitude = 0.0;
	double altitude = 0.0;
	if (!_get_eye_geodetic(latitude, longitude, altitude)) {
		last_error = "No eye point is available.";
		return;
	}

	_load_visible_tiles(latitude, longitude, altitude);
	_remember_refresh_eye(latitude, longitude, altitude);
}

void PXArchiveTerrain::_load_visible_tiles(double p_latitude, double p_longitude, double p_altitude) {
	_refresh_archives_for_eye(p_latitude, p_longitude);
	_drain_completed_tiles();

	std::vector<std::string> tiles_to_remove;
	for (const auto &entry : loaded_tiles) {
		if (!_aabb_is_near_eye(entry.second.aabb, p_latitude, p_longitude, p_altitude, cleanup_range_multiplier)) {
			tiles_to_remove.push_back(entry.first);
		}
	}
	for (const std::string &key : tiles_to_remove) {
		_remove_tile(key);
	}

	_queue_visible_tile_jobs(p_latitude, p_longitude, p_altitude);
	last_error = String();
}

} // namespace godot
