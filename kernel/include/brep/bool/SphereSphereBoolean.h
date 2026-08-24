#pragma once

#include "brep/bool/Context.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Builder.h"
#include "brep/Model.h"

namespace brep::boolean
{

/// Sphere ∪ Sphere (T3.8). Intersecting → two spherical caps; contained →
/// larger sphere; separate / coincident → soft-fail diagnostics.
[[nodiscard]] BooleanResult EvaluateSphereSphereBoolean(
    BooleanOp op, Model& model, const SphereSpec& a, const SphereSpec& b,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
