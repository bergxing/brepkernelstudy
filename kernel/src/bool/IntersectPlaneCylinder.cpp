#include "brep/bool/IntersectPlaneCylinder.h"

#include <cmath>
#include <string>

namespace brep::boolean
{

PlaneCylinderResult IntersectPlaneCylinder(
    const Point3d& plane_origin, const Vector3d& plane_normal,
    const Point3d& cyl_origin, const Vector3d& cyl_axis, double cyl_radius,
    const BooleanContext& ctx)
    {
  PlaneCylinderResult result;
  const double fuzzy = std::max(ctx.fuzzy, 0.0);

  if (!(cyl_radius > 0.0))
  {
    result.Diagnostics = "plane-cylinder: non-positive radius";
    return result;
  }
  const double n_len = plane_normal.norm();
  const double a_len = cyl_axis.norm();
  if (!(n_len > 0.0) || !(a_len > 0.0))
  {
    result.Diagnostics = "plane-cylinder: zero normal or axis";
    return result;
  }

  const Vector3d n = plane_normal / n_len;
  const Vector3d a = cyl_axis / a_len;
  result.Normal = n;

  const double cos_theta = n.dot(a);
  const double abs_cos = std::abs(cos_theta);

  // Axis ∥ plane.
  if (abs_cos <= fuzzy)
  {
    const double signed_d = n.dot(cyl_origin - plane_origin);
    const double dist = std::abs(signed_d);
    if (dist > cyl_radius + fuzzy)
    {
      result.status = PlaneCylinderStatus::Empty;
      result.Diagnostics = "plane-cylinder: parallel, no intersection";
      return result;
    }

    result.status = PlaneCylinderStatus::Lines;
    const Point3d foot = cyl_origin - n * signed_d;
    Vector3d lateral = n.cross(a);
    if (lateral.norm() < 1e-15)
    {
      const Vector3d ref =
          std::abs(a.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
      lateral = a.cross(ref);
    }
    lateral = lateral.normalized();

    if (dist >= cyl_radius - fuzzy)
    {
      const double s = (signed_d >= 0.0) ? 1.0 : -1.0;
      result.LinePoints.push_back(cyl_origin - n * (s * cyl_radius));
      result.LineDirs.push_back(a);
      result.Diagnostics = "plane-cylinder: parallel tangent (one line)";
    }
    else
    {
      const double h =
          std::sqrt(std::max(0.0, cyl_radius * cyl_radius - dist * dist));
      result.LinePoints.push_back(foot + lateral * h);
      result.LinePoints.push_back(foot - lateral * h);
      result.LineDirs.push_back(a);
      result.LineDirs.push_back(a);
      result.Diagnostics.clear();
    }
    return result;
  }

  // Axis meets plane at one point.
  const double t = n.dot(plane_origin - cyl_origin) / cos_theta;
  result.Center = cyl_origin + a * t;

  if (abs_cos >= 1.0 - fuzzy)
  {
    result.status = PlaneCylinderStatus::Circle;
    result.MajorRadius = cyl_radius;
    result.MinorRadius = cyl_radius;
    result.Diagnostics.clear();
    return result;
  }

  // Ellipse: minor = R, major = R / |sin(α)|, α = angle(axis, plane) ⇒
  // sin(α) = |n·a|.
  result.status = PlaneCylinderStatus::Ellipse;
  result.MinorRadius = cyl_radius;
  result.MajorRadius = cyl_radius / abs_cos;
  Vector3d minor = a.cross(n);
  if (minor.norm() < 1e-15)
  {
    result.Diagnostics = "plane-cylinder: ellipse frame degenerate";
    result.status = PlaneCylinderStatus::Empty;
    return result;
  }
  minor = minor.normalized();
  result.MinorDir = minor;
  result.MajorDir = n.cross(minor).normalized();
  result.Diagnostics.clear();
  return result;
}

PlaneCylinderResult IntersectPlaneCylinder(const PlaneSurface& plane,
                                             const CylinderSurface& cyl,
                                             const BooleanContext& ctx)
                                             {
  return IntersectPlaneCylinder(plane.Origin(), plane.Normal(0, 0),
                                  cyl.Origin(), cyl.Axis(), cyl.Radius(), ctx);
}

}  // namespace brep::boolean
