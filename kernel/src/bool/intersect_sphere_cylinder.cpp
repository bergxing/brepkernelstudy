#include "brep/bool/intersect_sphere_cylinder.hpp"

#include <cmath>
#include <string>

namespace brep::boolean {

SphereCylinderResult intersect_sphere_cylinder(
    const Point3d& sphere_center, double sphere_radius,
    const Point3d& cyl_origin, const Vector3d& cyl_axis, double cyl_radius,
    const BooleanContext& ctx) {
  SphereCylinderResult result;
  const double fuzzy = std::max(ctx.fuzzy, 0.0);

  if (!(sphere_radius > 0.0) || !(cyl_radius > 0.0)) {
    result.diagnostics = "sphere-cylinder: non-positive radius";
    return result;
  }
  const double a_len = cyl_axis.norm();
  if (!(a_len > 0.0)) {
    result.diagnostics = "sphere-cylinder: zero axis";
    return result;
  }
  const Vector3d a = cyl_axis / a_len;

  const Vector3d d = sphere_center - cyl_origin;
  const double along = d.dot(a);
  const Vector3d radial = d - a * along;
  const double dist_axis = radial.norm();

  // Off-axis general intersection is a space curve — defer.
  if (dist_axis > fuzzy) {
    result.status = SphereCylinderStatus::Unsupported;
    result.diagnostics =
        "sphere-cylinder: off-axis general curve not implemented";
    return result;
  }

  // Coaxial: plane(s) ⊥ axis at heights along where
  // (v - along)^2 + Rc^2 = Rs^2 ⇒ v = along ± sqrt(Rs^2 - Rc^2).
  if (sphere_radius + fuzzy < cyl_radius) {
    result.status = SphereCylinderStatus::Empty;
    result.diagnostics = "sphere-cylinder: sphere inside cylinder radius gap";
    return result;
  }

  const double disc =
      sphere_radius * sphere_radius - cyl_radius * cyl_radius;
  if (disc < -fuzzy * (sphere_radius + 1.0)) {
    result.status = SphereCylinderStatus::Empty;
    result.diagnostics = "sphere-cylinder: no real coaxial circles";
    return result;
  }

  result.status = SphereCylinderStatus::Circles;
  const double h = (disc <= fuzzy) ? 0.0 : std::sqrt(disc);
  auto push_circle = [&](double v) {
    SphereCylinderCircle c;
    c.center = cyl_origin + a * v;
    c.normal = a;
    c.radius = cyl_radius;
    result.circles.push_back(c);
  };

  if (h <= fuzzy) {
    push_circle(along);
    result.diagnostics = "sphere-cylinder: coaxial tangent circle";
  } else {
    push_circle(along - h);
    push_circle(along + h);
    result.diagnostics.clear();
  }
  return result;
}

SphereCylinderResult intersect_sphere_cylinder(const SphereSurface& sphere,
                                               const CylinderSurface& cyl,
                                               const BooleanContext& ctx) {
  return intersect_sphere_cylinder(sphere.center(), sphere.radius(),
                                   cyl.origin(), cyl.axis(), cyl.radius(), ctx);
}

}  // namespace brep::boolean
