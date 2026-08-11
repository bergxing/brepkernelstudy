#include "brep/bool/sphere_recognize.hpp"

#include "brep/geometry.hpp"

#include <cmath>

namespace brep::boolean {

std::optional<SphereSpec> recognize_analytic_sphere(const Body& body,
                                                    const BooleanContext& ctx) {
  const double tol = std::max(ctx.fuzzy, 1e-12);
  if (body.shells.size() != 1 || !body.shells[0]) return std::nullopt;
  const Shell& shell = *body.shells[0];
  if (!shell.closed || shell.faces.size() != 1) return std::nullopt;

  const Face* face = shell.faces[0];
  if (!face || !face->surface) return std::nullopt;
  const auto* sphere = dynamic_cast<const SphereSurface*>(face->surface);
  if (!sphere) return std::nullopt;
  if (!(sphere->radius() > tol)) return std::nullopt;

  SphereSpec spec;
  spec.center = sphere->center();
  spec.radius = sphere->radius();
  spec.tolerance = tol;
  spec.name = body.name.empty() ? "sphere" : body.name;
  return spec;
}

}  // namespace brep::boolean
