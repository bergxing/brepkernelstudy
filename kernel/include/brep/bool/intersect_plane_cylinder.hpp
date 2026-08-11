#pragma once

#include "brep/bool/context.hpp"
#include "brep/geometry.hpp"
#include "brep/math.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace brep::boolean {

enum class PlaneCylinderStatus : std::uint8_t {
  Circle = 0,   ///< Axis perpendicular to plane
  Ellipse = 1,  ///< Axis oblique to plane (non-parallel)
  Lines = 2,    ///< Axis parallel to plane: 0/1/2 generators
  Empty = 3
};

struct PlaneCylinderResult {
  PlaneCylinderStatus status{PlaneCylinderStatus::Empty};
  Point3d center{};           ///< Circle/ellipse center (plane ∩ axis)
  Vector3d normal{};          ///< Plane unit normal
  Vector3d major_dir{};       ///< Ellipse major axis dir (or unused)
  Vector3d minor_dir{};       ///< Ellipse minor axis dir
  double major_radius{0.0};   ///< Circle radius or ellipse semi-major
  double minor_radius{0.0};   ///< Circle: same as major; ellipse semi-minor
  std::vector<Point3d> line_points;  ///< One point per generator when Lines
  std::vector<Vector3d> line_dirs;   ///< Unit dirs (= ±cylinder axis)
  std::string diagnostics;

  [[nodiscard]] bool is_circle() const noexcept {
    return status == PlaneCylinderStatus::Circle;
  }
};

[[nodiscard]] PlaneCylinderResult intersect_plane_cylinder(
    const PlaneSurface& plane, const CylinderSurface& cyl,
    const BooleanContext& ctx = {});

[[nodiscard]] PlaneCylinderResult intersect_plane_cylinder(
    const Point3d& plane_origin, const Vector3d& plane_normal,
    const Point3d& cyl_origin, const Vector3d& cyl_axis, double cyl_radius,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
