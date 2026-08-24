#include "brep/bool/IntersectPlaneSphere.h"

#include <cmath>
#include <string>

namespace brep::boolean
{

PlaneSphereResult IntersectPlaneSphere(const Point3d& plane_origin,
                                         const Vector3d& plane_normal,
                                         const Point3d& sphere_center,
                                         double sphere_radius,
                                         const BooleanContext& ctx)
                                         {
  PlaneSphereResult result;
  const double fuzzy = std::max(ctx.fuzzy, 0.0);

  if (!(sphere_radius > 0.0))
  {
    result.status = PlaneSphereStatus::Empty;
    result.Diagnostics = "plane-sphere: non-positive sphere radius";
    return result;
  }

  const double n_len = plane_normal.norm();
  if (!(n_len > 0.0))
  {
    result.status = PlaneSphereStatus::Empty;
    result.Diagnostics = "plane-sphere: zero-length plane normal";
    return result;
  }

  const Vector3d n = plane_normal / n_len;
  const double dist = n.dot(sphere_center - plane_origin);
  const double abs_dist = std::abs(dist);

  result.Normal = n;
  result.Center = sphere_center - n * dist;

  if (abs_dist > sphere_radius + fuzzy)
  {
    result.status = PlaneSphereStatus::Empty;
    result.Diagnostics = "plane-sphere: plane misses sphere";
    return result;
  }

  if (abs_dist >= sphere_radius - fuzzy)
  {
    result.status = PlaneSphereStatus::Point;
    result.Radius = 0.0;
    result.Diagnostics = "plane-sphere: tangent (point)";
    return result;
  }

  result.status = PlaneSphereStatus::Circle;
  result.Radius = std::sqrt(std::max(0.0, sphere_radius * sphere_radius -
                                              abs_dist * abs_dist));
  result.Diagnostics.clear();
  return result;
}

PlaneSphereResult IntersectPlaneSphere(const PlaneSurface& plane,
                                         const SphereSurface& sphere,
                                         const BooleanContext& ctx)
                                         {
  return IntersectPlaneSphere(plane.Origin(), plane.Normal(0, 0),
                                sphere.Center(), sphere.Radius(), ctx);
}

}  // namespace brep::boolean
