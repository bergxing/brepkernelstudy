#pragma once

#include "brep/bool/Context.h"
#include "brep/Geometry.h"
#include "brep/Math.h"

#include <cstdint>
#include <string>

namespace brep::boolean
{

enum class PlanePlaneStatus : std::uint8_t
{
  Line = 0,       ///< Distinct intersecting planes → infinite line
  Parallel = 1,   ///< Distinct parallel planes (no intersection)
  Coincident = 2  ///< Same plane within fuzzy (infinite coincidence)
};

struct PlanePlaneResult
{
  PlanePlaneStatus status{PlanePlaneStatus::Parallel};
  Point3d Point{};       ///< A point on the intersection line when status==Line
  Vector3d Direction{};  ///< Unit direction of the line when status==Line
  std::string Diagnostics;

  [[nodiscard]] bool IsLine() const noexcept
  {
    return status == PlanePlaneStatus::Line;
  }
};

/// Intersect two infinite planes (design T2.0.4 / IntTools minimum).
[[nodiscard]] PlanePlaneResult IntersectPlanePlane(
    const PlaneSurface& a, const PlaneSurface& b,
    const BooleanContext& ctx = {});

/// Overload taking explicit origins/normals (need not be unit).
[[nodiscard]] PlanePlaneResult IntersectPlanePlane(
    const Point3d& origin_a, const Vector3d& normal_a, const Point3d& origin_b,
    const Vector3d& normal_b, const BooleanContext& ctx = {});

}  // namespace brep::boolean
