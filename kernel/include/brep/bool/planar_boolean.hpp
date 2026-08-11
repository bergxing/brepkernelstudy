#pragma once

#include "brep/bool/context.hpp"
#include "brep/bool/planar_recognize.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
#include "brep/builder.hpp"
#include "brep/model.hpp"

namespace brep::boolean {

/// Prism × axis-aligned box Fuse/Cut/Common via arrangement grid.
/// `prism_is_a`: if true, operand A is the prism; otherwise A is the box.
[[nodiscard]] BooleanResult evaluate_prism_box_boolean(
    BooleanOp op, Model& model, const PlanarPrismSpec& prism,
    const BoxSpec& box, bool prism_is_a, const BooleanContext& ctx = {});

}  // namespace brep::boolean
