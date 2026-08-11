#pragma once

#include "brep/bool/context.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
#include "brep/builder.hpp"
#include "brep/model.hpp"
#include "brep/topology.hpp"

namespace brep::boolean {

/// Sphere × axis-aligned box boolean. MVP: Intersect when the sphere center is
/// a box corner and the box fully contains that octant of the ball (→ ⅛ ball).
[[nodiscard]] BooleanResult evaluate_sphere_box_boolean(
    BooleanOp op, Model& model, const SphereSpec& sphere, const BoxSpec& box,
    bool sphere_is_a, const BooleanContext& ctx = {});

}  // namespace brep::boolean
