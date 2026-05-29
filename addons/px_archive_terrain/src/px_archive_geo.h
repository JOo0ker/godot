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
	Aabb estimate_tile_aabb(const TileCandidate &p_candidate, double p_vertical_range) const;

private:
	double origin_latitude = 0.0;
	double origin_longitude = 0.0;
	double origin_altitude = 0.0;
};

} // namespace px_archive_terrain
