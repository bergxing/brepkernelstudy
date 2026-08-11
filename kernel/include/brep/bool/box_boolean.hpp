#pragma once

#include "brep/bool/context.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
#include "brep/model.hpp"
#include "brep/topology.hpp"

namespace brep::boolean {

/// Box–box Fuse / Cut / Common → closed B-Rep (all Outer loops).
[[nodiscard]] BooleanResult evaluate_box_boolean(BooleanOp op, Model& model,
                                                 const Body& a, const Body& b,
                                                 const BooleanContext& ctx);

}  // namespace brep::boolean
