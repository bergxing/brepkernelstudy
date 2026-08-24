#pragma once

#include "brep/bool/Context.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Model.h"
#include "brep/Topology.h"

#include <functional>
#include <memory>

namespace brep::boolean
{

/// Boolean evaluation backend (OCCT-inspired pipeline; self-hosted).
class IBooleanEvaluator
{
 public:
  virtual ~IBooleanEvaluator() = default;

  [[nodiscard]] virtual BooleanResult Evaluate(BooleanOp op, Model& model,
                                               const Body& a, const Body& b,
                                               const BooleanContext& ctx) = 0;
};

/// Phase 2.0.1 skeleton stub (always fails). Prefer
/// `MakeDefaultBooleanEvaluator` for production (box–box path).
[[nodiscard]] std::unique_ptr<IBooleanEvaluator> MakeStubBooleanEvaluator();

using BooleanEvaluatorFactory =
    std::function<std::shared_ptr<IBooleanEvaluator>()>;

/// Optional override used by Part / XL load (tests inject a fake until needed).
void SetBooleanEvaluatorFactory(BooleanEvaluatorFactory factory);
/// Default: axis-aligned box–box Fuse/Cut/Common; other combos fail soft.
[[nodiscard]] std::shared_ptr<IBooleanEvaluator> MakeDefaultBooleanEvaluator();

}  // namespace brep::boolean
