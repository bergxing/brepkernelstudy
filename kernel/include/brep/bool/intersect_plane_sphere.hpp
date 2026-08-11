#pragma once

#include "brep/bool/context.hpp"
#include "brep/geometry.hpp"
#include "brep/math.hpp"

#include <cstdint>
#include <string>

namespace brep::boolean {

enum class PlaneSphereStatus : std::uint8_t {
  Circle = 0,  ///< Plane cuts sphere in a circle (radius > 0)
  Point = 1,   ///< Tangency within fuzzy
  Empty = 2    ///< No real intersection
};

struct PlaneSphereResult {
  PlaneSphereStatus status{PlaneSphereStatus::Empty};
  Point3d center{};   ///< Circle center (or tangency point)
  Vector3d normal{};  ///< Unit plane normal (circle plane)
  double radius{0.0};
  std::string diagnostics;

  [[nodiscard]] bool is_circle() const noexcept {
    return status == PlaneSphereStatus::Circle;
  }
};

[[nodiscard]] PlaneSphereResult intersect_plane_sphere(
    const PlaneSurface& plane, const SphereSurface& sphere,
    const BooleanContext& ctx = {});

[[nodiscard]] PlaneSphereResult intersect_plane_sphere(
    const Point3d& plane_origin, const Vector3d& plane_normal,
    const Point3d& sphere_center, double sphere_radius,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
