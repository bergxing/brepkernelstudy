#include "brep/bool/intersect_plane_sphere.hpp"

#include <cmath>
#include <string>

namespace brep::boolean {

PlaneSphereResult intersect_plane_sphere(const Point3d& plane_origin,
                                         const Vector3d& plane_normal,
                                         const Point3d& sphere_center,
                                         double sphere_radius,
                                         const BooleanContext& ctx) {
  PlaneSphereResult result;
  const double fuzzy = std::max(ctx.fuzzy, 0.0);

  if (!(sphere_radius > 0.0)) {
    result.status = PlaneSphereStatus::Empty;
    result.diagnostics = "plane-sphere: non-positive sphere radius";
    return result;
  }

  const double n_len = plane_normal.norm();
  if (!(n_len > 0.0)) {
    result.status = PlaneSphereStatus::Empty;
    result.diagnostics = "plane-sphere: zero-length plane normal";
    return result;
  }

  const Vector3d n = plane_normal / n_len;
  const double dist = n.dot(sphere_center - plane_origin);
  const double abs_dist = std::abs(dist);

  result.normal = n;
  result.center = sphere_center - n * dist;

  if (abs_dist > sphere_radius + fuzzy) {
    result.status = PlaneSphereStatus::Empty;
    result.diagnostics = "plane-sphere: plane misses sphere";
    return result;
  }

  if (abs_dist >= sphere_radius - fuzzy) {
    result.status = PlaneSphereStatus::Point;
    result.radius = 0.0;
    result.diagnostics = "plane-sphere: tangent (point)";
    return result;
  }

  result.status = PlaneSphereStatus::Circle;
  result.radius = std::sqrt(std::max(0.0, sphere_radius * sphere_radius -
                                              abs_dist * abs_dist));
  result.diagnostics.clear();
  return result;
}

PlaneSphereResult intersect_plane_sphere(const PlaneSurface& plane,
                                         const SphereSurface& sphere,
                                         const BooleanContext& ctx) {
  return intersect_plane_sphere(plane.origin(), plane.normal(0, 0),
                                sphere.center(), sphere.radius(), ctx);
}

}  // namespace brep::boolean
