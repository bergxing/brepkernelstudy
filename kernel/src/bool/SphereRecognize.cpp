#include "brep/bool/SphereRecognize.h"

#include "brep/Geometry.h"

#include <cmath>

namespace brep::boolean
{

std::optional<SphereSpec> RecognizeAnalyticSphere(const Body& body,
                                                    const BooleanContext& ctx)
{
  const double tol = std::max(ctx.fuzzy, 1e-12);
  if (body.Shells.size() != 1 || !body.Shells[0]) return std::nullopt;
  const Shell& shell = *body.Shells[0];
  if (!shell.Closed || shell.Faces.size() != 1) return std::nullopt;

  const Face* face = shell.Faces[0];
  if (!face || !face->Surface) return std::nullopt;
  const auto* sphere = dynamic_cast<const SphereSurface*>(face->Surface);
  if (!sphere) return std::nullopt;
  if (!(sphere->Radius() > tol)) return std::nullopt;

  SphereSpec spec;
  spec.Center = sphere->Center();
  spec.Radius = sphere->Radius();
  spec.Tolerance = tol;
  spec.Name = body.Name.empty() ? "sphere" : body.Name;
  return spec;
}

}  // namespace brep::boolean
