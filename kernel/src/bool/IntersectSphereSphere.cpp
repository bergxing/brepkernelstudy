#include "brep/bool/IntersectSphereSphere.h"

#include <cmath>
#include <string>

namespace brep::boolean
{

SphereSphereResult IntersectSphereSphere(const Point3d& center_a,
                                           double radius_a,
                                           const Point3d& center_b,
                                           double radius_b,
                                           const BooleanContext& ctx)
                                           {
  SphereSphereResult result;
  const double fuzzy = std::max(ctx.fuzzy, 0.0);

  if (!(radius_a > 0.0) || !(radius_b > 0.0))
  {
    result.status = SphereSphereStatus::Separate;
    result.Diagnostics = "sphere-sphere: non-positive radius";
    return result;
  }

  const Vector3d delta = center_b - center_a;
  const double d = delta.norm();

  if (d <= fuzzy && std::abs(radius_a - radius_b) <= fuzzy)
  {
    result.status = SphereSphereStatus::Coincident;
    result.Diagnostics = "sphere-sphere: coincident spheres";
    return result;
  }

  if (d <= fuzzy)
  {
    // Concentric, different radii.
    result.status = SphereSphereStatus::Contained;
    result.Diagnostics = "sphere-sphere: concentric, different radii";
    return result;
  }

  const Vector3d n = delta / d;
  result.Normal = n;

  // External separate
  if (d > radius_a + radius_b + fuzzy)
  {
    result.status = SphereSphereStatus::Separate;
    result.Diagnostics = "sphere-sphere: separate";
    return result;
  }

  // One inside the other without touch
  if (d + std::min(radius_a, radius_b) < std::max(radius_a, radius_b) - fuzzy)
  {
    result.status = SphereSphereStatus::Contained;
    result.Diagnostics = "sphere-sphere: one contained in the other";
    return result;
  }

  // Tangency (external or internal)
  if (std::abs(d - (radius_a + radius_b)) <= fuzzy ||
      std::abs(d - std::abs(radius_a - radius_b)) <= fuzzy)
  {
    result.status = SphereSphereStatus::Point;
    // External: point along n at radius_a from a.
    // Internal: same formula with signed distance along line of centers.
    const double t =
        (d * d + radius_a * radius_a - radius_b * radius_b) / (2.0 * d);
    result.Center = center_a + n * t;
    result.Radius = 0.0;
    result.Diagnostics = "sphere-sphere: tangent (point)";
    return result;
  }

  // Circle of intersection
  const double t =
      (d * d + radius_a * radius_a - radius_b * radius_b) / (2.0 * d);
  const double h2 = radius_a * radius_a - t * t;
  if (h2 < 0.0 && std::abs(h2) <= fuzzy * (radius_a + 1.0))
  {
    result.status = SphereSphereStatus::Point;
    result.Center = center_a + n * t;
    result.Radius = 0.0;
    result.Diagnostics = "sphere-sphere: near-tangent";
    return result;
  }
  if (h2 < 0.0)
  {
    result.status = SphereSphereStatus::Separate;
    result.Diagnostics = "sphere-sphere: numerical miss";
    return result;
  }

  result.status = SphereSphereStatus::Circle;
  result.Center = center_a + n * t;
  result.Radius = std::sqrt(h2);
  result.Diagnostics.clear();
  return result;
}

SphereSphereResult IntersectSphereSphere(const SphereSurface& a,
                                           const SphereSurface& b,
                                           const BooleanContext& ctx)
                                           {
  return IntersectSphereSphere(a.Center(), a.Radius(), b.Center(), b.Radius(),
                                 ctx);
}

}  // namespace brep::boolean
