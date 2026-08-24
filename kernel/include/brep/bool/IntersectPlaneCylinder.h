#pragma once

#include "brep/bool/Context.h"
#include "brep/Geometry.h"
#include "brep/Math.h"

#include <cstdint>
#include <string>
#include <vector>

namespace brep::boolean
{

enum class PlaneCylinderStatus : std::uint8_t
{
  Circle = 0,   ///< Axis perpendicular to plane
  Ellipse = 1,  ///< Axis oblique to plane (non-parallel)
  Lines = 2,    ///< Axis parallel to plane: 0/1/2 generators
  Empty = 3
};

struct PlaneCylinderResult
{
  PlaneCylinderStatus status{PlaneCylinderStatus::Empty};
  Point3d Center{};           ///< Circle/ellipse center (plane ∩ axis)
  Vector3d Normal{};          ///< Plane unit normal
  Vector3d MajorDir{};        ///< Ellipse major axis dir (or unused)
  Vector3d MinorDir{};        ///< Ellipse minor axis dir
  double MajorRadius{0.0};    ///< Circle radius or ellipse semi-major
  double MinorRadius{0.0};    ///< Circle: same as major; ellipse semi-minor
  std::vector<Point3d> LinePoints;  ///< One point per generator when Lines
  std::vector<Vector3d> LineDirs;   ///< Unit dirs (= ±cylinder axis)
  std::string Diagnostics;

  [[nodiscard]] bool IsCircle() const noexcept
  {
    return status == PlaneCylinderStatus::Circle;
  }
};

[[nodiscard]] PlaneCylinderResult IntersectPlaneCylinder(
    const PlaneSurface& plane, const CylinderSurface& cyl,
    const BooleanContext& ctx = {});

[[nodiscard]] PlaneCylinderResult IntersectPlaneCylinder(
    const Point3d& plane_origin, const Vector3d& plane_normal,
    const Point3d& cyl_origin, const Vector3d& cyl_axis, double cyl_radius,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
