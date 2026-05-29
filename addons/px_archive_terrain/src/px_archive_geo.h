#pragma once

#include "px_archive_core.h"

namespace px_archive_terrain {

struct Aabb {
	Vec3d position;
	Vec3d size;
};

class GeoReference {
public:
	static constexpr double DEG_TO_RAD = 0.017453292519943295;
	static constexpr double DEG_TO_METERS = 111194.92664455874;

	void set_origin(double p_latitude, double p_longitude, double p_altitude);

	double get_origin_latitude() const;
	double get_origin_longitude() const;
	double get_origin_altitude() const;

	Vec3d geo_to_local(double p_latitude, double p_longitude, double p_altitude) const;
	void local_to_geo(const Vec3d &p_local, double &r_latitude, double &r_longitude, double &r_altitude) const;
	Vec3d source_tile_vertex_to_local(const TileCandidate &p_candidate, const Vec3f &p_vertex) const;
	Aabb estimate_tile_aabb(const TileCandidate &p_candidate, double p_vertical_range) const;

private:
	static Vec3d geo_to_ecef(double p_latitude, double p_longitude, double p_altitude);
	static Vec3d ecef_to_local_tangent(const Vec3d &p_point, double p_origin_latitude, double p_origin_longitude, double p_origin_altitude);
	static Vec3d local_tangent_to_ecef(const Vec3d &p_local, double p_origin_latitude, double p_origin_longitude, double p_origin_altitude);
	static void ecef_to_geo(const Vec3d &p_point, double &r_latitude, double &r_longitude, double &r_altitude);
	static Vec3d tile_local_to_ecef(double p_center_latitude, double p_center_longitude, const Vec3f &p_vertex);

	double origin_latitude = 0.0;
	double origin_longitude = 0.0;
	double origin_altitude = 0.0;
};

} // namespace px_archive_terrain
