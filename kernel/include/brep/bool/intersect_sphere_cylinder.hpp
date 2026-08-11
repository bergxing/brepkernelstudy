#pragma once

#include "brep/bool/context.hpp"
#include "brep/geometry.hpp"
#include "brep/math.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace brep::boolean {

enum class SphereCylinderStatus : std::uint8_t {
  Circles = 0,     ///< Sphere center on axis → 0/1/2 circles
  Empty = 1,       ///< No real intersection
  Unsupported = 2  ///< General (off-axis) curve not implemented yet
};

struct SphereCylinderCircle {
  Point3d center{};
  Vector3d normal{};
  double radius{0.0};
};

struct SphereCylinderResult {
  SphereCylinderStatus status{SphereCylinderStatus::Empty};
  std::vector<SphereCylinderCircle> circles;
  std::string diagnostics;

  [[nodiscard]] bool has_circles() const noexcept {
    return status == SphereCylinderStatus::Circles && !circles.empty();
  }
};

[[nodiscard]] SphereCylinderResult intersect_sphere_cylinder(
    const SphereSurface& sphere, const CylinderSurface& cyl,
    const BooleanContext& ctx = {});

[[nodiscard]] SphereCylinderResult intersect_sphere_cylinder(
    const Point3d& sphere_center, double sphere_radius,
    const Point3d& cyl_origin, const Vector3d& cyl_axis, double cyl_radius,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
