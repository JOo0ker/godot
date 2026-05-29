#include "px_archive_geo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace px_archive_terrain {
namespace {

constexpr double EARTH_RADIUS_METERS = 6371000.0;

Vec3d add(const Vec3d &p_lhs, const Vec3d &p_rhs) {
	return { p_lhs.x + p_rhs.x, p_lhs.y + p_rhs.y, p_lhs.z + p_rhs.z };
}

Vec3d mul(double p_scalar, const Vec3d &p_value) {
	return { p_scalar * p_value.x, p_scalar * p_value.y, p_scalar * p_value.z };
}

double dot(const Vec3d &p_lhs, const Vec3d &p_rhs) {
	return p_lhs.x * p_rhs.x + p_lhs.y * p_rhs.y + p_lhs.z * p_rhs.z;
}

} // namespace

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
	return ecef_to_local_tangent(geo_to_ecef(p_latitude, p_longitude, p_altitude), origin_latitude, origin_longitude, origin_altitude);
}

void GeoReference::local_to_geo(const Vec3d &p_local, double &r_latitude, double &r_longitude, double &r_altitude) const {
	ecef_to_geo(local_tangent_to_ecef(p_local, origin_latitude, origin_longitude, origin_altitude), r_latitude, r_longitude, r_altitude);
}

Vec3d GeoReference::source_tile_vertex_to_local(const TileCandidate &p_candidate, const Vec3f &p_vertex) const {
	const Vec3d ecef = tile_local_to_ecef(p_candidate.latitude, p_candidate.longitude, p_vertex);
	return ecef_to_local_tangent(ecef, origin_latitude, origin_longitude, origin_altitude);
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

Vec3d GeoReference::geo_to_ecef(double p_latitude, double p_longitude, double p_altitude) {
	const double lat = p_latitude * DEG_TO_RAD;
	const double lon = p_longitude * DEG_TO_RAD;
	const double radius = EARTH_RADIUS_METERS + p_altitude;
	return {
		radius * std::cos(lat) * std::sin(lon),
		radius * std::sin(lat),
		radius * std::cos(lat) * std::cos(lon),
	};
}

Vec3d GeoReference::ecef_to_local_tangent(const Vec3d &p_point, double p_origin_latitude, double p_origin_longitude, double p_origin_altitude) {
	const double lat = p_origin_latitude * DEG_TO_RAD;
	const double lon = p_origin_longitude * DEG_TO_RAD;
	const Vec3d origin = geo_to_ecef(p_origin_latitude, p_origin_longitude, p_origin_altitude);
	const Vec3d delta{ p_point.x - origin.x, p_point.y - origin.y, p_point.z - origin.z };
	const Vec3d east{ std::cos(lon), 0.0, -std::sin(lon) };
	const Vec3d north{ -std::sin(lat) * std::sin(lon), std::cos(lat), -std::sin(lat) * std::cos(lon) };
	const Vec3d up{ std::cos(lat) * std::sin(lon), std::sin(lat), std::cos(lat) * std::cos(lon) };

	return {
		dot(delta, east),
		dot(delta, up),
		dot(delta, north),
	};
}

Vec3d GeoReference::local_tangent_to_ecef(const Vec3d &p_local, double p_origin_latitude, double p_origin_longitude, double p_origin_altitude) {
	const double lat = p_origin_latitude * DEG_TO_RAD;
	const double lon = p_origin_longitude * DEG_TO_RAD;
	const Vec3d origin = geo_to_ecef(p_origin_latitude, p_origin_longitude, p_origin_altitude);
	const Vec3d east{ std::cos(lon), 0.0, -std::sin(lon) };
	const Vec3d north{ -std::sin(lat) * std::sin(lon), std::cos(lat), -std::sin(lat) * std::cos(lon) };
	const Vec3d up{ std::cos(lat) * std::sin(lon), std::sin(lat), std::cos(lat) * std::cos(lon) };

	return add(add(add(origin, mul(p_local.x, east)), mul(p_local.z, north)), mul(p_local.y, up));
}

void GeoReference::ecef_to_geo(const Vec3d &p_point, double &r_latitude, double &r_longitude, double &r_altitude) {
	const double length_xz = std::sqrt(p_point.x * p_point.x + p_point.z * p_point.z);
	const double length_xyz = std::sqrt(p_point.x * p_point.x + p_point.y * p_point.y + p_point.z * p_point.z);
	r_latitude = std::atan2(p_point.y, length_xz) / DEG_TO_RAD;
	r_longitude = std::atan2(p_point.x, p_point.z) / DEG_TO_RAD;
	r_altitude = length_xyz - EARTH_RADIUS_METERS;
}

Vec3d GeoReference::tile_local_to_ecef(double p_center_latitude, double p_center_longitude, const Vec3f &p_vertex) {
	const double lat = p_center_latitude * DEG_TO_RAD;
	const double lon = p_center_longitude * DEG_TO_RAD;
	const Vec3d center = geo_to_ecef(p_center_latitude, p_center_longitude, 0.0);
	const Vec3d east{ std::cos(lon), 0.0, -std::sin(lon) };
	const Vec3d north{ -std::sin(lat) * std::sin(lon), std::cos(lat), -std::sin(lat) * std::cos(lon) };
	const Vec3d up{ std::cos(lat) * std::sin(lon), std::sin(lat), std::cos(lat) * std::cos(lon) };

	return add(add(add(center, mul(p_vertex.x, east)), mul(p_vertex.y, north)), mul(p_vertex.z, up));
}

} // namespace px_archive_terrain
