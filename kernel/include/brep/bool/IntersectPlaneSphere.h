#pragma once

#include "brep/bool/Context.h"
#include "brep/Geometry.h"
#include "brep/Math.h"

#include <cstdint>
#include <string>

namespace brep::boolean
{

enum class PlaneSphereStatus : std::uint8_t
{
  Circle = 0,  ///< Plane cuts sphere in a circle (radius > 0)
  Point = 1,   ///< Tangency within fuzzy
  Empty = 2    ///< No real intersection
};

struct PlaneSphereResult
{
  PlaneSphereStatus status{PlaneSphereStatus::Empty};
  Point3d Center{};   ///< Circle center (or tangency point)
  Vector3d Normal{};  ///< Unit plane normal (circle plane)
  double Radius{0.0};
  std::string Diagnostics;

  [[nodiscard]] bool IsCircle() const noexcept
  {
    return status == PlaneSphereStatus::Circle;
  }
};

[[nodiscard]] PlaneSphereResult IntersectPlaneSphere(
    const PlaneSurface& plane, const SphereSurface& sphere,
    const BooleanContext& ctx = {});

[[nodiscard]] PlaneSphereResult IntersectPlaneSphere(
    const Point3d& plane_origin, const Vector3d& plane_normal,
    const Point3d& sphere_center, double sphere_radius,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
