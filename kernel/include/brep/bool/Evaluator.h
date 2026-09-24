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

/// Skeleton stub (always fails). Prefer `MakeDefaultBooleanEvaluator`.
[[nodiscard]] std::unique_ptr<IBooleanEvaluator> MakeStubBooleanEvaluator();

using BooleanEvaluatorFactory =
    std::function<std::shared_ptr<IBooleanEvaluator>()>;

/// Optional override used by tests until all call sites use explicit injection.
/// Prefer constructing Document/Part with a shared evaluator (ADR 0007).
[[deprecated("Use Document/Part constructor injection or TestContainer override")]]
void SetBooleanEvaluatorFactory(BooleanEvaluatorFactory factory);
/// Default: general BooleanPipeline (ADR 0008); no analytic fast paths registered.
[[nodiscard]] std::shared_ptr<IBooleanEvaluator> MakeDefaultBooleanEvaluator();

}  // namespace brep::boolean
