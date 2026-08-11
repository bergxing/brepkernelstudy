#pragma once

#include "brep/bool/context.hpp"
#include "brep/geometry.hpp"
#include "brep/math.hpp"

#include <cstdint>
#include <string>

namespace brep::boolean {

enum class SphereSphereStatus : std::uint8_t {
  Circle = 0,      ///< Distinct intersecting spheres → circle
  Point = 1,       ///< External or internal tangency
  Separate = 2,    ///< Disjoint, no touch
  Contained = 3,   ///< One strictly inside the other, no touch
  Coincident = 4   ///< Same center and radius within fuzzy
};

struct SphereSphereResult {
  SphereSphereStatus status{SphereSphereStatus::Separate};
  Point3d center{};
  Vector3d normal{};
  double radius{0.0};
  std::string diagnostics;

  [[nodiscard]] bool is_circle() const noexcept {
    return status == SphereSphereStatus::Circle;
  }
};

[[nodiscard]] SphereSphereResult intersect_sphere_sphere(
    const SphereSurface& a, const SphereSurface& b,
    const BooleanContext& ctx = {});

[[nodiscard]] SphereSphereResult intersect_sphere_sphere(
    const Point3d& center_a, double radius_a, const Point3d& center_b,
    double radius_b, const BooleanContext& ctx = {});

}  // namespace brep::boolean
