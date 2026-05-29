#include "px_archive_terrain.h"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

namespace godot {
namespace {

Vector3 to_godot_vector(const px_archive_terrain::Vec3d &p_value) {
	return Vector3(static_cast<real_t>(p_value.x), static_cast<real_t>(p_value.y), static_cast<real_t>(p_value.z));
}

String tile_key(const std::string &p_archive_path, const std::string &p_base_name) {
	return String((p_archive_path + "|" + p_base_name).c_str());
}

std::string tile_key_std(const std::string &p_archive_path, const std::string &p_base_name) {
	return p_archive_path + "|" + p_base_name;
}

} // namespace

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
	ClassDB::bind_method(D_METHOD("set_use_camera_eye", "enabled"), &PXArchiveTerrain::set_use_camera_eye);
	ClassDB::bind_method(D_METHOD("is_using_camera_eye"), &PXArchiveTerrain::is_using_camera_eye);
	ClassDB::bind_method(D_METHOD("set_eye_geodetic", "latitude", "longitude", "altitude"), &PXArchiveTerrain::set_eye_geodetic);
	ClassDB::bind_method(D_METHOD("set_eye_latitude", "latitude"), &PXArchiveTerrain::set_eye_latitude);
	ClassDB::bind_method(D_METHOD("get_eye_latitude"), &PXArchiveTerrain::get_eye_latitude);
	ClassDB::bind_method(D_METHOD("set_eye_longitude", "longitude"), &PXArchiveTerrain::set_eye_longitude);
	ClassDB::bind_method(D_METHOD("get_eye_longitude"), &PXArchiveTerrain::get_eye_longitude);
	ClassDB::bind_method(D_METHOD("set_eye_altitude", "altitude"), &PXArchiveTerrain::set_eye_altitude);
	ClassDB::bind_method(D_METHOD("get_eye_altitude"), &PXArchiveTerrain::get_eye_altitude);
	ClassDB::bind_method(D_METHOD("set_lod", "lod"), &PXArchiveTerrain::set_lod);
	ClassDB::bind_method(D_METHOD("get_lod"), &PXArchiveTerrain::get_lod);
	ClassDB::bind_method(D_METHOD("set_texture_lod", "lod"), &PXArchiveTerrain::set_texture_lod);
	ClassDB::bind_method(D_METHOD("get_texture_lod"), &PXArchiveTerrain::get_texture_lod);
	ClassDB::bind_method(D_METHOD("set_max_tiles_per_update", "max_tiles"), &PXArchiveTerrain::set_max_tiles_per_update);
	ClassDB::bind_method(D_METHOD("get_max_tiles_per_update"), &PXArchiveTerrain::get_max_tiles_per_update);
	ClassDB::bind_method(D_METHOD("set_max_vertices_per_tile", "max_vertices"), &PXArchiveTerrain::set_max_vertices_per_tile);
	ClassDB::bind_method(D_METHOD("get_max_vertices_per_tile"), &PXArchiveTerrain::get_max_vertices_per_tile);
	ClassDB::bind_method(D_METHOD("set_archive_neighborhood", "neighborhood"), &PXArchiveTerrain::set_archive_neighborhood);
	ClassDB::bind_method(D_METHOD("get_archive_neighborhood"), &PXArchiveTerrain::get_archive_neighborhood);
	ClassDB::bind_method(D_METHOD("set_vertical_range", "range"), &PXArchiveTerrain::set_vertical_range);
	ClassDB::bind_method(D_METHOD("get_vertical_range"), &PXArchiveTerrain::get_vertical_range);
	ClassDB::bind_method(D_METHOD("set_mesh_scale", "scale"), &PXArchiveTerrain::set_mesh_scale);
	ClassDB::bind_method(D_METHOD("get_mesh_scale"), &PXArchiveTerrain::get_mesh_scale);
	ClassDB::bind_method(D_METHOD("set_auto_refresh_interval", "interval"), &PXArchiveTerrain::set_auto_refresh_interval);
	ClassDB::bind_method(D_METHOD("get_auto_refresh_interval"), &PXArchiveTerrain::get_auto_refresh_interval);
	ClassDB::bind_method(D_METHOD("set_auto_refresh_min_distance", "distance"), &PXArchiveTerrain::set_auto_refresh_min_distance);
	ClassDB::bind_method(D_METHOD("get_auto_refresh_min_distance"), &PXArchiveTerrain::get_auto_refresh_min_distance);
	ClassDB::bind_method(D_METHOD("set_load_textures", "enabled"), &PXArchiveTerrain::set_load_textures);
	ClassDB::bind_method(D_METHOD("is_loading_textures"), &PXArchiveTerrain::is_loading_textures);
	ClassDB::bind_method(D_METHOD("set_auto_refresh", "enabled"), &PXArchiveTerrain::set_auto_refresh);
	ClassDB::bind_method(D_METHOD("is_auto_refresh_enabled"), &PXArchiveTerrain::is_auto_refresh_enabled);
	ClassDB::bind_method(D_METHOD("refresh_visible_tiles"), &PXArchiveTerrain::refresh_visible_tiles);
	ClassDB::bind_method(D_METHOD("clear_tiles"), &PXArchiveTerrain::clear_tiles);
	ClassDB::bind_method(D_METHOD("get_loaded_archive_count"), &PXArchiveTerrain::get_loaded_archive_count);
	ClassDB::bind_method(D_METHOD("get_loaded_tile_count"), &PXArchiveTerrain::get_loaded_tile_count);
	ClassDB::bind_method(D_METHOD("get_last_error"), &PXArchiveTerrain::get_last_error);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "terrain_folder", PROPERTY_HINT_DIR), "set_terrain_folder", "get_terrain_folder");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "origin_latitude"), "set_origin_latitude", "get_origin_latitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "origin_longitude"), "set_origin_longitude", "get_origin_longitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "origin_altitude"), "set_origin_altitude", "get_origin_altitude");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_camera_eye"), "set_use_camera_eye", "is_using_camera_eye");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "eye_latitude"), "set_eye_latitude", "get_eye_latitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "eye_longitude"), "set_eye_longitude", "get_eye_longitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "eye_altitude"), "set_eye_altitude", "get_eye_altitude");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod", PROPERTY_HINT_RANGE, "0,3,1"), "set_lod", "get_lod");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_lod", PROPERTY_HINT_RANGE, "0,3,1"), "set_texture_lod", "get_texture_lod");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_tiles_per_update", PROPERTY_HINT_RANGE, "1,4096,1"), "set_max_tiles_per_update", "get_max_tiles_per_update");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_vertices_per_tile", PROPERTY_HINT_RANGE, "3,2000000,1"), "set_max_vertices_per_tile", "get_max_vertices_per_tile");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "archive_neighborhood", PROPERTY_HINT_RANGE, "0,4,1"), "set_archive_neighborhood", "get_archive_neighborhood");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vertical_range"), "set_vertical_range", "get_vertical_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mesh_scale"), "set_mesh_scale", "get_mesh_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_refresh_interval", PROPERTY_HINT_RANGE, "0.05,10,0.05"), "set_auto_refresh_interval", "get_auto_refresh_interval");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "auto_refresh_min_distance", PROPERTY_HINT_RANGE, "0,100000,100"), "set_auto_refresh_min_distance", "get_auto_refresh_min_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "load_textures"), "set_load_textures", "is_loading_textures");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_refresh"), "set_auto_refresh", "is_auto_refresh_enabled");
}

void PXArchiveTerrain::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		set_process(auto_refresh);
	} else if (p_what == NOTIFICATION_PROCESS) {
		if (auto_refresh) {
			auto_refresh_elapsed += get_process_delta_time();
			if (auto_refresh_elapsed >= auto_refresh_interval) {
				auto_refresh_elapsed = 0.0;
				double latitude = 0.0;
				double longitude = 0.0;
				double altitude = 0.0;
				if (_get_eye_geodetic(latitude, longitude, altitude) && _should_auto_refresh(latitude, longitude, altitude)) {
					_load_visible_tiles(latitude, longitude, altitude);
					_remember_refresh_eye(latitude, longitude, altitude);
				}
			}
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		set_process(false);
	}
}

void PXArchiveTerrain::set_terrain_folder(const String &p_path) {
	terrain_folder = p_path;
	loaded_archives.clear();
	has_last_refresh_eye = false;
	has_pending_visible_tiles = false;
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
	has_last_refresh_eye = false;
	has_pending_visible_tiles = false;
	_clear_tiles();
}

void PXArchiveTerrain::set_use_camera_eye(bool p_enabled) {
	use_camera_eye = p_enabled;
}

bool PXArchiveTerrain::is_using_camera_eye() const {
	return use_camera_eye;
}

void PXArchiveTerrain::set_eye_geodetic(double p_latitude, double p_longitude, double p_altitude) {
	eye_latitude = p_latitude;
	eye_longitude = p_longitude;
	eye_altitude = p_altitude;
	use_camera_eye = false;
	has_last_refresh_eye = false;
	has_pending_visible_tiles = false;
}

void PXArchiveTerrain::set_eye_latitude(double p_value) {
	set_eye_geodetic(p_value, eye_longitude, eye_altitude);
}

double PXArchiveTerrain::get_eye_latitude() const {
	return eye_latitude;
}

void PXArchiveTerrain::set_eye_longitude(double p_value) {
	set_eye_geodetic(eye_latitude, p_value, eye_altitude);
}

double PXArchiveTerrain::get_eye_longitude() const {
	return eye_longitude;
}

void PXArchiveTerrain::set_eye_altitude(double p_value) {
	set_eye_geodetic(eye_latitude, eye_longitude, p_value);
}

double PXArchiveTerrain::get_eye_altitude() const {
	return eye_altitude;
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
}

int PXArchiveTerrain::get_max_tiles_per_update() const {
	return max_tiles_per_update;
}

void PXArchiveTerrain::set_max_vertices_per_tile(int p_max_vertices) {
	max_vertices_per_tile = std::max(3, p_max_vertices);
}

int PXArchiveTerrain::get_max_vertices_per_tile() const {
	return max_vertices_per_tile;
}

void PXArchiveTerrain::set_archive_neighborhood(int p_neighborhood) {
	archive_neighborhood = std::max(0, p_neighborhood);
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

void PXArchiveTerrain::set_mesh_scale(double p_scale) {
	mesh_scale = std::max(0.000001, p_scale);
	_clear_tiles();
}

double PXArchiveTerrain::get_mesh_scale() const {
	return mesh_scale;
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

void PXArchiveTerrain::set_load_textures(bool p_enabled) {
	load_textures = p_enabled;
	_force_refresh_after_tiles_changed();
}

bool PXArchiveTerrain::is_loading_textures() const {
	return load_textures;
}

void PXArchiveTerrain::set_auto_refresh(bool p_enabled) {
	auto_refresh = p_enabled;
	auto_refresh_elapsed = auto_refresh_interval;
	if (is_inside_tree()) {
		set_process(auto_refresh);
	}
}

bool PXArchiveTerrain::is_auto_refresh_enabled() const {
	return auto_refresh;
}

void PXArchiveTerrain::refresh_visible_tiles() {
	_load_visible_tiles();
}

void PXArchiveTerrain::clear_tiles() {
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
}

void PXArchiveTerrain::_remove_tile(const std::string &p_key) {
	const auto found = loaded_tiles.find(p_key);
	if (found == loaded_tiles.end()) {
		return;
	}

	if (found->second.node) {
		found->second.node->queue_free();
	}
	loaded_tiles.erase(found);
}

void PXArchiveTerrain::_refresh_archives_for_eye(double p_latitude, double p_longitude) {
	const std::vector<std::string> desired_paths = px_archive_terrain::ArchiveReader::build_neighbor_archive_paths(to_utf8(terrain_folder), p_latitude, p_longitude, archive_neighborhood);
	std::unordered_set<std::string> desired_set(desired_paths.begin(), desired_paths.end());

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

		auto reader = std::make_unique<px_archive_terrain::ArchiveReader>();
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
	if (!use_camera_eye) {
		r_latitude = eye_latitude;
		r_longitude = eye_longitude;
		r_altitude = eye_altitude;
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

bool PXArchiveTerrain::_should_auto_refresh(double p_latitude, double p_longitude, double p_altitude) const {
	if (!has_last_refresh_eye) {
		return true;
	}
	if (has_pending_visible_tiles) {
		return true;
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
	has_last_refresh_eye = true;
}

void PXArchiveTerrain::_force_refresh_after_tiles_changed() {
	_clear_tiles();
	has_last_refresh_eye = false;
	has_pending_visible_tiles = true;
	auto_refresh_elapsed = auto_refresh_interval;
}

bool PXArchiveTerrain::_aabb_intersects_camera(const px_archive_terrain::Aabb &p_aabb) const {
	const Viewport *viewport = get_viewport();
	if (!viewport) {
		return false;
	}

	Camera3D *camera = viewport->get_camera_3d();
	if (!camera) {
		return false;
	}

	const AABB local_aabb(to_godot_vector(p_aabb.position), to_godot_vector(p_aabb.size));
	const AABB global_aabb = get_global_transform().xform(local_aabb);
	const TypedArray<Plane> frustum = camera->get_frustum();

	for (int i = 0; i < frustum.size(); i++) {
		const Plane plane = frustum[i];
		const Vector3 support = global_aabb.get_support(-plane.normal);
		if (plane.is_point_over(support)) {
			return false;
		}
	}

	return true;
}

px_archive_terrain::Aabb PXArchiveTerrain::_tile_mesh_aabb(const px_archive_terrain::TileData &p_tile) const {
	const px_archive_terrain::Vec3d center = geo_reference.geo_to_local(p_tile.candidate.latitude, p_tile.candidate.longitude, 0.0);
	if (p_tile.dem_points.empty()) {
		return { center, { 0.0, 0.0, 0.0 } };
	}

	const double scale = mesh_scale;
	px_archive_terrain::Vec3d min_point{
		center.x + p_tile.dem_points[0].vertex.x * scale,
		center.y + p_tile.dem_points[0].vertex.z * scale,
		center.z + p_tile.dem_points[0].vertex.y * scale,
	};
	px_archive_terrain::Vec3d max_point = min_point;

	for (size_t i = 1; i < p_tile.dem_points.size(); i++) {
		const px_archive_terrain::Vec3d point{
			center.x + p_tile.dem_points[i].vertex.x * scale,
			center.y + p_tile.dem_points[i].vertex.z * scale,
			center.z + p_tile.dem_points[i].vertex.y * scale,
		};
		min_point.x = std::min(min_point.x, point.x);
		min_point.y = std::min(min_point.y, point.y);
		min_point.z = std::min(min_point.z, point.z);
		max_point.x = std::max(max_point.x, point.x);
		max_point.y = std::max(max_point.y, point.y);
		max_point.z = std::max(max_point.z, point.z);
	}

	return {
		min_point,
		{ max_point.x - min_point.x, max_point.y - min_point.y, max_point.z - min_point.z },
	};
}

MeshInstance3D *PXArchiveTerrain::_create_tile_mesh(const px_archive_terrain::TileData &p_tile) {
	const px_archive_terrain::Vec3d center = geo_reference.geo_to_local(p_tile.candidate.latitude, p_tile.candidate.longitude, 0.0);
	const double scale = mesh_scale;

	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	vertices.resize(static_cast<int>(p_tile.dem_points.size()));
	normals.resize(static_cast<int>(p_tile.dem_points.size()));
	uvs.resize(static_cast<int>(p_tile.dem_points.size()));
	for (int i = 0; i < static_cast<int>(p_tile.dem_points.size()); i++) {
		const px_archive_terrain::TileDemPoint &point = p_tile.dem_points[static_cast<size_t>(i)];
		vertices.set(i, Vector3(
				static_cast<real_t>(point.vertex.x * scale),
				static_cast<real_t>(point.vertex.z * scale),
				static_cast<real_t>(point.vertex.y * scale)));
		normals.set(i, Vector3(
				static_cast<real_t>(point.normal.x),
				static_cast<real_t>(point.normal.z),
				static_cast<real_t>(point.normal.y))
							   .normalized());
		uvs.set(i, Vector2(point.uv[0], point.uv[1]));
	}

	indices.resize(static_cast<int>(p_tile.indices.size()));
	for (int i = 0; i < static_cast<int>(p_tile.indices.size()); i++) {
		indices.set(i, static_cast<int32_t>(p_tile.indices[static_cast<size_t>(i)]));
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
	if (p_tile.has_texture && !p_tile.texture_data.empty()) {
		PackedByteArray texture_bytes;
		texture_bytes.resize(static_cast<int>(p_tile.texture_data.size()));
		for (int i = 0; i < static_cast<int>(p_tile.texture_data.size()); i++) {
			texture_bytes.set(i, p_tile.texture_data[static_cast<size_t>(i)]);
		}

		Ref<Image> image;
		image.instantiate();
		if (image->load_dds_from_buffer(texture_bytes) == OK) {
			if (image->is_compressed()) {
				image->decompress();
			}
			if (image->get_format() != Image::FORMAT_RGBA8) {
				image->convert(Image::FORMAT_RGBA8);
			}
			Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
			Ref<StandardMaterial3D> material;
			material.instantiate();
			material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
			material->set_albedo(Color(1.0, 1.0, 1.0, 1.0));
			material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			mesh_instance->set_surface_override_material(0, material);
		}
	}
	mesh_instance->set_position(Vector3(static_cast<real_t>(center.x), static_cast<real_t>(center.y), static_cast<real_t>(center.z)));
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
	(void)p_altitude;
	_refresh_archives_for_eye(p_latitude, p_longitude);

	std::vector<std::string> tiles_to_remove;
	for (const auto &entry : loaded_tiles) {
		if (!_aabb_intersects_camera(entry.second.aabb)) {
			tiles_to_remove.push_back(entry.first);
		}
	}
	for (const std::string &key : tiles_to_remove) {
		_remove_tile(key);
	}

	int loaded_this_update = 0;
	for (auto &archive_entry : loaded_archives) {
		const std::string &archive_path = archive_entry.first;
		px_archive_terrain::ArchiveReader *reader = archive_entry.second.reader.get();
		if (!reader) {
			continue;
		}

		for (const px_archive_terrain::TileCandidate &candidate : reader->get_candidates()) {
			const std::string key = tile_key_std(archive_path, candidate.base_name);
			if (loaded_tiles.find(key) != loaded_tiles.end()) {
				continue;
			}

			const px_archive_terrain::Aabb estimate = geo_reference.estimate_tile_aabb(candidate, vertical_range);
			if (!_aabb_intersects_camera(estimate)) {
				continue;
			}

			if (loaded_this_update >= max_tiles_per_update) {
				last_error = String();
				has_pending_visible_tiles = true;
				return;
			}

			px_archive_terrain::TileData tile;
			if (!reader->read_tile(candidate, lod, texture_lod, static_cast<uint32_t>(max_vertices_per_tile), tile, load_textures)) {
				last_error = from_utf8(reader->get_error_text());
				continue;
			}

			const px_archive_terrain::Aabb mesh_aabb = _tile_mesh_aabb(tile);
			if (!_aabb_intersects_camera(mesh_aabb)) {
				continue;
			}

			MeshInstance3D *mesh_instance = _create_tile_mesh(tile);
			add_child(mesh_instance);
			loaded_tiles.emplace(key, LoadedTile{ mesh_instance, mesh_aabb, archive_path });
			loaded_this_update++;
		}
	}

	last_error = String();
	has_pending_visible_tiles = false;
}

} // namespace godot
