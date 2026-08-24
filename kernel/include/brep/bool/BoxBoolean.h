#pragma once

#include "brep/bool/Context.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Model.h"
#include "brep/Topology.h"

namespace brep::boolean
{

/// Box–box Fuse / Cut / Common → closed B-Rep (all Outer loops).
[[nodiscard]] BooleanResult EvaluateBoxBoolean(BooleanOp op, Model& model,
                                                 const Body& a, const Body& b,
                                                 const BooleanContext& ctx);

}  // namespace brep::boolean
