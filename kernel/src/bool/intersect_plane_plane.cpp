#include "brep/bool/intersect_plane_plane.hpp"

#include <cmath>
#include <string>

namespace brep::boolean {
namespace {

[[nodiscard]] double signed_plane_distance(const Point3d& origin,
                                           const Vector3d& unit_normal,
                                           const Point3d& p) {
  return unit_normal.dot(p - origin);
}

}  // namespace

PlanePlaneResult intersect_plane_plane(const Point3d& origin_a,
                                       const Vector3d& normal_a,
                                       const Point3d& origin_b,
                                       const Vector3d& normal_b,
                                       const BooleanContext& ctx) {
  PlanePlaneResult result;
  const double fuzzy = std::max(ctx.fuzzy, 0.0);

  const double na_len = normal_a.norm();
  const double nb_len = normal_b.norm();
  if (!(na_len > 0.0) || !(nb_len > 0.0)) {
    result.status = PlanePlaneStatus::Parallel;
    result.diagnostics = "plane-plane: zero-length normal";
    return result;
  }

  const Vector3d n1 = normal_a / na_len;
  const Vector3d n2 = normal_b / nb_len;
  const Vector3d cross = n1.cross(n2);
  const double sin_theta = cross.norm();

  // Near-parallel (including opposite normals): |n1×n2| ≈ |sin θ|.
  if (sin_theta <= fuzzy) {
    const double dist = std::abs(signed_plane_distance(origin_a, n1, origin_b));
    if (dist <= fuzzy) {
      result.status = PlanePlaneStatus::Coincident;
      result.diagnostics = "plane-plane: coincident within fuzzy";
    } else {
      result.status = PlanePlaneStatus::Parallel;
      result.diagnostics = "plane-plane: parallel distinct planes";
    }
    return result;
  }

  // Line direction.
  const Vector3d dir = cross / sin_theta;

  // Plane equations: n·x = d with d = n·origin (origin as vector from world 0).
  const Vector3d oa{origin_a.eigen()};
  const Vector3d ob{origin_b.eigen()};
  const double d1 = n1.dot(oa);
  const double d2 = n2.dot(ob);

  // Point on line: ((d1 n2 - d2 n1) × (n1 × n2)) / |n1 × n2|^2
  const Vector3d point_v =
      (d1 * n2 - d2 * n1).cross(cross) / (sin_theta * sin_theta);

  result.status = PlanePlaneStatus::Line;
  result.point = Point3d{point_v.eigen()};
  result.direction = dir;
  result.diagnostics.clear();
  return result;
}

PlanePlaneResult intersect_plane_plane(const PlaneSurface& a,
                                       const PlaneSurface& b,
                                       const BooleanContext& ctx) {
  return intersect_plane_plane(a.origin(), a.normal(0, 0), b.origin(),
                               b.normal(0, 0), ctx);
}

}  // namespace brep::boolean
