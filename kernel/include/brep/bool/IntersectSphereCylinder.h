#pragma once

#include "brep/bool/Context.h"
#include "brep/Geometry.h"
#include "brep/Math.h"

#include <cstdint>
#include <string>
#include <vector>

namespace brep::boolean
{

enum class SphereCylinderStatus : std::uint8_t
{
  Circles = 0,     ///< Sphere center on axis → 0/1/2 circles
  Empty = 1,       ///< No real intersection
  Unsupported = 2  ///< General (off-axis) curve not implemented yet
};

struct SphereCylinderCircle
{
  Point3d Center{};
  Vector3d Normal{};
  double Radius{0.0};
};

struct SphereCylinderResult
{
  SphereCylinderStatus status{SphereCylinderStatus::Empty};
  std::vector<SphereCylinderCircle> Circles;
  std::string Diagnostics;

  [[nodiscard]] bool HasCircles() const noexcept
  {
    return status == SphereCylinderStatus::Circles && !Circles.empty();
  }
};

[[nodiscard]] SphereCylinderResult IntersectSphereCylinder(
    const SphereSurface& sphere, const CylinderSurface& cyl,
    const BooleanContext& ctx = {});

[[nodiscard]] SphereCylinderResult IntersectSphereCylinder(
    const Point3d& sphere_center, double sphere_radius,
    const Point3d& cyl_origin, const Vector3d& cyl_axis, double cyl_radius,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
