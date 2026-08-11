#pragma once

#include "brep/bool/context.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
#include "brep/builder.hpp"
#include "brep/model.hpp"

namespace brep::boolean {

/// Sphere ∪ Sphere (T3.8). Intersecting → two spherical caps; contained →
/// larger sphere; separate / coincident → soft-fail diagnostics.
[[nodiscard]] BooleanResult evaluate_sphere_sphere_boolean(
    BooleanOp op, Model& model, const SphereSpec& a, const SphereSpec& b,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
