#pragma once

#include "px_archive_core.h"
#include "px_archive_geo.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace godot {

class PXArchiveTerrain : public Node3D {
	GDCLASS(PXArchiveTerrain, Node3D)

	struct ArchiveState {
		std::shared_ptr<px_archive_terrain::ArchiveReader> reader;
	};

	struct LoadedTile {
		MeshInstance3D *node = nullptr;
		px_archive_terrain::Aabb aabb;
		std::string archive_path;
	};

	struct TileLoadJob {
		std::string key;
		std::string archive_path;
		px_archive_terrain::TileCandidate candidate;
		uint64_t generation = 0;
		int lod = 0;
		int texture_lod = 0;
		uint32_t max_vertices = 0;
		double origin_latitude = 0.0;
		double origin_longitude = 0.0;
		double origin_altitude = 0.0;
	};

	struct TileDisplayData {
		std::string base_name;
		px_archive_terrain::Aabb aabb;
		std::vector<float> vertices;
		std::vector<float> normals;
		std::vector<float> uvs;
		std::vector<int32_t> indices;
		Ref<Image> texture_image;
		bool has_texture = false;
	};

	struct TileLoadResult {
		std::string key;
		std::string archive_path;
		TileDisplayData display;
		std::string error_text;
		bool success = false;
		uint64_t generation = 0;
	};

	String terrain_folder;
	int lod = 0;
	int texture_lod = 0;
	int max_tiles_per_update = 64;
	int max_tile_jobs_per_refresh = 160;
	int max_tile_attach_per_frame = 1;
	int max_vertices_per_tile = 200000;
	int archive_neighborhood = 1;
	double vertical_range = 10000.0;
	double loading_range = 150000.0;
	double cleanup_range_multiplier = 1.1;
	double max_tile_attach_time_ms = 1.0;
	double auto_refresh_interval = 0.1;
	double auto_refresh_min_distance = 1000.0;
	double auto_refresh_min_angle = 1.0;
	int worker_thread_count = 2;
	double auto_refresh_elapsed = 0.0;
	double last_refresh_latitude = 0.0;
	double last_refresh_longitude = 0.0;
	double last_refresh_altitude = 0.0;
	bool has_last_refresh_eye = false;
	bool has_editor_eye_override = false;
	bool has_editor_camera_frustum = false;
	bool has_last_refresh_camera_basis = false;
	Vector3 editor_eye_global_position;
	Vector3 last_refresh_camera_forward;
	Vector3 last_refresh_camera_up;
	Array editor_camera_frustum;
	String last_error;
	uint64_t load_generation = 0;
	bool worker_stop = false;

	px_archive_terrain::GeoReference geo_reference;
	std::unordered_map<std::string, ArchiveState> loaded_archives;
	std::unordered_map<std::string, LoadedTile> loaded_tiles;
	std::unordered_set<std::string> queued_tiles;
	std::unordered_set<std::string> failed_tiles;
	std::vector<std::thread> workers;
	std::mutex worker_mutex;
	std::condition_variable worker_cv;
	std::deque<TileLoadJob> pending_jobs;
	std::deque<TileLoadResult> completed_jobs;

	static std::string to_utf8(const String &p_value);
	static String from_utf8(const std::string &p_value);
	static bool _prepare_tile_display_data(const px_archive_terrain::TileData &p_tile, const px_archive_terrain::GeoReference &p_geo_reference, TileDisplayData &r_display, std::string &r_error_text);

	void _clear_tiles();
	void _remove_tile(const std::string &p_key);
	void _refresh_archives_for_eye(double p_latitude, double p_longitude);
	bool _get_eye_geodetic(double &r_latitude, double &r_longitude, double &r_altitude) const;
	bool _get_camera_basis(Vector3 &r_forward, Vector3 &r_up) const;
	bool _should_auto_refresh(double p_latitude, double p_longitude, double p_altitude) const;
	void _remember_refresh_eye(double p_latitude, double p_longitude, double p_altitude);
	bool _aabb_intersects_camera(const px_archive_terrain::Aabb &p_aabb) const;
	bool _aabb_is_near_eye(const px_archive_terrain::Aabb &p_aabb, double p_latitude, double p_longitude, double p_altitude, double p_multiplier) const;
	MeshInstance3D *_create_tile_mesh(const TileDisplayData &p_tile);
	void _load_visible_tiles();
	void _load_visible_tiles(double p_latitude, double p_longitude, double p_altitude);
	void _force_refresh_after_tiles_changed();
	void _request_auto_refresh();
	bool _refresh_from_current_eye(bool p_respect_refresh_gate);
	void _start_workers();
	void _stop_workers();
	void _worker_loop();
	void _clear_pending_loads();
	void _drain_completed_tiles();
	void _queue_visible_tile_jobs(double p_latitude, double p_longitude, double p_altitude);
	bool _has_pending_loads();

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

	void set_editor_eye_global_position(const Vector3 &p_global_position);
	void set_editor_camera_state(const Vector3 &p_global_position, const Array &p_frustum);
	void clear_editor_eye_override();

	void set_lod(int p_lod);
	int get_lod() const;
	void set_texture_lod(int p_lod);
	int get_texture_lod() const;
	void set_max_tiles_per_update(int p_max_tiles);
	int get_max_tiles_per_update() const;
	void set_max_tile_jobs_per_refresh(int p_max_tiles);
	int get_max_tile_jobs_per_refresh() const;
	void set_max_tile_attach_per_frame(int p_max_tiles);
	int get_max_tile_attach_per_frame() const;
	void set_max_vertices_per_tile(int p_max_vertices);
	int get_max_vertices_per_tile() const;
	void set_archive_neighborhood(int p_neighborhood);
	int get_archive_neighborhood() const;
	void set_vertical_range(double p_range);
	double get_vertical_range() const;
	void set_loading_range(double p_range);
	double get_loading_range() const;
	void set_cleanup_range_multiplier(double p_multiplier);
	double get_cleanup_range_multiplier() const;
	void set_max_tile_attach_time_ms(double p_milliseconds);
	double get_max_tile_attach_time_ms() const;
	void set_auto_refresh_interval(double p_interval);
	double get_auto_refresh_interval() const;
	void set_auto_refresh_min_distance(double p_distance);
	double get_auto_refresh_min_distance() const;
	void set_auto_refresh_min_angle(double p_degrees);
	double get_auto_refresh_min_angle() const;
	void set_worker_thread_count(int p_count);
	int get_worker_thread_count() const;

	void refresh_visible_tiles();
	void clear_tiles();
	int get_loaded_archive_count() const;
	int get_loaded_tile_count() const;
	String get_last_error() const;
};

} // namespace godot
