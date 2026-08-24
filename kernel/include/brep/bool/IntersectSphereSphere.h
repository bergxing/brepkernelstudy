#pragma once

#include "brep/bool/Context.h"
#include "brep/Geometry.h"
#include "brep/Math.h"

#include <cstdint>
#include <string>

namespace brep::boolean
{

enum class SphereSphereStatus : std::uint8_t
{
  Circle = 0,      ///< Distinct intersecting spheres → circle
  Point = 1,       ///< External or internal tangency
  Separate = 2,    ///< Disjoint, no touch
  Contained = 3,   ///< One strictly inside the other, no touch
  Coincident = 4   ///< Same center and radius within fuzzy
};

struct SphereSphereResult
{
  SphereSphereStatus status{SphereSphereStatus::Separate};
  Point3d Center{};
  Vector3d Normal{};
  double Radius{0.0};
  std::string Diagnostics;

  [[nodiscard]] bool IsCircle() const noexcept
  {
    return status == SphereSphereStatus::Circle;
  }
};

[[nodiscard]] SphereSphereResult IntersectSphereSphere(
    const SphereSurface& a, const SphereSurface& b,
    const BooleanContext& ctx = {});

[[nodiscard]] SphereSphereResult IntersectSphereSphere(
    const Point3d& center_a, double radius_a, const Point3d& center_b,
    double radius_b, const BooleanContext& ctx = {});

}  // namespace brep::boolean
