#pragma once

#include "px_archive_core.h"
#include "px_archive_geo.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class PXArchiveTerrain : public Node3D {
	GDCLASS(PXArchiveTerrain, Node3D)

	struct ArchiveState {
		std::unique_ptr<px_archive_terrain::ArchiveReader> reader;
	};

	struct LoadedTile {
		MeshInstance3D *node = nullptr;
		px_archive_terrain::Aabb aabb;
		std::string archive_path;
	};

	String terrain_folder;
	int lod = 0;
	int texture_lod = 0;
	int max_tiles_per_update = 64;
	int max_vertices_per_tile = 200000;
	int archive_neighborhood = 1;
	double vertical_range = 10000.0;
	double mesh_scale = 1.0;
	double auto_refresh_interval = 0.5;
	double auto_refresh_min_distance = 1000.0;
	bool auto_refresh = true;
	bool load_textures = true;
	bool use_camera_eye = true;
	double eye_latitude = 0.0;
	double eye_longitude = 0.0;
	double eye_altitude = 0.0;
	double auto_refresh_elapsed = 0.0;
	double last_refresh_latitude = 0.0;
	double last_refresh_longitude = 0.0;
	double last_refresh_altitude = 0.0;
	bool has_last_refresh_eye = false;
	bool has_pending_visible_tiles = false;
	bool has_editor_eye_override = false;
	bool has_editor_camera_frustum = false;
	Vector3 editor_eye_global_position;
	Array editor_camera_frustum;
	String last_error;

	px_archive_terrain::GeoReference geo_reference;
	std::unordered_map<std::string, ArchiveState> loaded_archives;
	std::unordered_map<std::string, LoadedTile> loaded_tiles;

	static std::string to_utf8(const String &p_value);
	static String from_utf8(const std::string &p_value);

	void _clear_tiles();
	void _remove_tile(const std::string &p_key);
	void _refresh_archives_for_eye(double p_latitude, double p_longitude);
	bool _get_eye_geodetic(double &r_latitude, double &r_longitude, double &r_altitude) const;
	bool _should_auto_refresh(double p_latitude, double p_longitude, double p_altitude) const;
	void _remember_refresh_eye(double p_latitude, double p_longitude, double p_altitude);
	bool _aabb_intersects_camera(const px_archive_terrain::Aabb &p_aabb) const;
	px_archive_terrain::Aabb _tile_mesh_aabb(const px_archive_terrain::TileData &p_tile) const;
	MeshInstance3D *_create_tile_mesh(const px_archive_terrain::TileData &p_tile);
	void _load_visible_tiles();
	void _load_visible_tiles(double p_latitude, double p_longitude, double p_altitude);
	void _force_refresh_after_tiles_changed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_terrain_folder(const String &p_path);
	String get_terrain_folder() const;

	void set_origin_latitude(double p_value);
	double get_origin_latitude() const;
	void set_origin_longitude(double p_value);
	double get_origin_longitude() const;
	void set_origin_altitude(double p_value);
	double get_origin_altitude() const;
	void set_origin(double p_latitude, double p_longitude, double p_altitude);

	void set_use_camera_eye(bool p_enabled);
	bool is_using_camera_eye() const;
	void set_eye_geodetic(double p_latitude, double p_longitude, double p_altitude);
	void set_editor_eye_global_position(const Vector3 &p_global_position);
	void set_editor_camera_state(const Vector3 &p_global_position, const Array &p_frustum);
	void clear_editor_eye_override();
	void set_eye_latitude(double p_value);
	double get_eye_latitude() const;
	void set_eye_longitude(double p_value);
	double get_eye_longitude() const;
	void set_eye_altitude(double p_value);
	double get_eye_altitude() const;

	void set_lod(int p_lod);
	int get_lod() const;
	void set_texture_lod(int p_lod);
	int get_texture_lod() const;
	void set_max_tiles_per_update(int p_max_tiles);
	int get_max_tiles_per_update() const;
	void set_max_vertices_per_tile(int p_max_vertices);
	int get_max_vertices_per_tile() const;
	void set_archive_neighborhood(int p_neighborhood);
	int get_archive_neighborhood() const;
	void set_vertical_range(double p_range);
	double get_vertical_range() const;
	void set_mesh_scale(double p_scale);
	double get_mesh_scale() const;
	void set_auto_refresh_interval(double p_interval);
	double get_auto_refresh_interval() const;
	void set_auto_refresh_min_distance(double p_distance);
	double get_auto_refresh_min_distance() const;
	void set_load_textures(bool p_enabled);
	bool is_loading_textures() const;
	void set_auto_refresh(bool p_enabled);
	bool is_auto_refresh_enabled() const;

	void refresh_visible_tiles();
	void clear_tiles();
	int get_loaded_archive_count() const;
	int get_loaded_tile_count() const;
	String get_last_error() const;
};

} // namespace godot
