#pragma once

#include "brep/bool/Evaluator.h"
#include "brep/bool/Pipeline.h"

#include <memory>

namespace brep::boolean
{

/// General boolean pipeline evaluator (ADR 0008).
class PipelineBooleanEvaluator final : public IBooleanEvaluator
{
 public:
  explicit PipelineBooleanEvaluator(std::unique_ptr<BooleanPipeline> pipeline);

  [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model, const Body& a,
                                     const Body& b,
                                     const BooleanContext& ctx) override;

 private:
  std::unique_ptr<BooleanPipeline> m_pipeline;
};

[[nodiscard]] std::shared_ptr<IBooleanEvaluator> MakePipelineBooleanEvaluator();

/// Back-compat alias.
using CompositeBooleanEvaluator = PipelineBooleanEvaluator;
[[nodiscard]] std::shared_ptr<IBooleanEvaluator> MakeCompositeBooleanEvaluator();

}  // namespace brep::boolean
