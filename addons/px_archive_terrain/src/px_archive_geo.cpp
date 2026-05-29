#include "px_archive_geo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace px_archive_terrain {

void GeoReference::set_origin(double p_latitude, double p_longitude, double p_altitude) {
	origin_latitude = p_latitude;
	origin_longitude = p_longitude;
	origin_altitude = p_altitude;
}

double GeoReference::get_origin_latitude() const {
	return origin_latitude;
}

double GeoReference::get_origin_longitude() const {
	return origin_longitude;
}

double GeoReference::get_origin_altitude() const {
	return origin_altitude;
}

Vec3d GeoReference::geo_to_local(double p_latitude, double p_longitude, double p_altitude) const {
	const double average_latitude = 0.5 * (p_latitude + origin_latitude);
	const double latitude_correction = std::max(0.001, std::cos(average_latitude * DEG_TO_RAD));

	return {
		(p_longitude - origin_longitude) * DEG_TO_METERS * latitude_correction,
		p_altitude - origin_altitude,
		(p_latitude - origin_latitude) * DEG_TO_METERS,
	};
}

void GeoReference::local_to_geo(const Vec3d &p_local, double &r_latitude, double &r_longitude, double &r_altitude) const {
	r_latitude = origin_latitude + p_local.z / DEG_TO_METERS;
	const double average_latitude = 0.5 * (r_latitude + origin_latitude);
	const double latitude_correction = std::max(0.001, std::cos(average_latitude * DEG_TO_RAD));
	r_longitude = origin_longitude + p_local.x / (DEG_TO_METERS * latitude_correction);
	r_altitude = origin_altitude + p_local.y;
}

Aabb GeoReference::estimate_tile_aabb(const TileCandidate &p_candidate, double p_vertical_range) const {
	const int subdivision_count = std::max(1, p_candidate.depth + 1);
	const int clamped_subdivision = std::min(subdivision_count, 20);
	const double degree_span = 180.0 / static_cast<double>(uint64_t(1) << clamped_subdivision);
	const double latitude_correction = std::max(0.001, std::cos(p_candidate.latitude * DEG_TO_RAD));
	const double half_x = std::max(0.05, degree_span * DEG_TO_METERS * latitude_correction * 0.5);
	const double half_z = std::max(0.05, degree_span * DEG_TO_METERS * 0.5);
	const double half_y = std::max(1.0, p_vertical_range);
	const Vec3d center = geo_to_local(p_candidate.latitude, p_candidate.longitude, 0.0);

	return {
		{ center.x - half_x, center.y - half_y, center.z - half_z },
		{ half_x * 2.0, half_y * 2.0, half_z * 2.0 },
	};
}

} // namespace px_archive_terrain
