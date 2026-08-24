#pragma once

#include "brep/bool/Context.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Builder.h"
#include "brep/Model.h"
#include "brep/Topology.h"

namespace brep::boolean
{

/// Sphere × axis-aligned box boolean.
/// - Intersect / Sphere−Box / Sphere∪Box: sphere center at a box corner with
///   the inward octant of the ball inside the box (⅛ / ⅞ / box+spherical patch)
/// - Union also: either body contains the other → clone the outer body
[[nodiscard]] BooleanResult EvaluateSphereBoxBoolean(
    BooleanOp op, Model& model, const SphereSpec& sphere, const BoxSpec& box,
    bool sphere_is_a, const BooleanContext& ctx = {});

}  // namespace brep::boolean
