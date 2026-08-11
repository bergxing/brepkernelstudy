#pragma once

#include "brep/bool/context.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
#include "brep/builder.hpp"
#include "brep/model.hpp"
#include "brep/topology.hpp"

namespace brep::boolean {

/// Sphere × axis-aligned box boolean.
/// - Intersect / Sphere−Box / Sphere∪Box: sphere center at a box corner with
///   the inward octant of the ball inside the box (⅛ / ⅞ / box+spherical patch)
/// - Union also: either body contains the other → clone the outer body
[[nodiscard]] BooleanResult evaluate_sphere_box_boolean(
    BooleanOp op, Model& model, const SphereSpec& sphere, const BoxSpec& box,
    bool sphere_is_a, const BooleanContext& ctx = {});

}  // namespace brep::boolean
