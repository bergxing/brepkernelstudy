#pragma once

#include "brep/bool/Context.h"
#include "brep/bool/PlanarRecognize.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Builder.h"
#include "brep/Model.h"

namespace brep::boolean
{

/// Prism × axis-aligned box Fuse/Cut/Common via arrangement grid.
/// `prism_is_a`: if true, operand A is the prism; otherwise A is the box.
[[nodiscard]] BooleanResult EvaluatePrismBoxBoolean(
    BooleanOp op, Model& model, const PlanarPrismSpec& prism,
    const BoxSpec& box, bool prism_is_a, const BooleanContext& ctx = {});

}  // namespace brep::boolean
