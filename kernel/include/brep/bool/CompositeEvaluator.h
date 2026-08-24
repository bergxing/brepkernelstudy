#pragma once

#include "brep/bool/Evaluator.h"
#include "brep/bool/FastPath.h"
#include "brep/bool/Pipeline.h"

#include <memory>

namespace brep::boolean
{

/// Tries registered analytic fast paths when `CanHandle` is true; otherwise
/// runs the general BooleanPipeline.
class CompositeBooleanEvaluator final : public IBooleanEvaluator
{
 public:
  CompositeBooleanEvaluator(AnalyticFastPathRegistry fastPaths,
                            std::unique_ptr<BooleanPipeline> pipeline);

  [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model, const Body& a,
                                     const Body& b,
                                     const BooleanContext& ctx) override;

 private:
  AnalyticFastPathRegistry m_fastPaths;
  std::unique_ptr<BooleanPipeline> m_pipeline;
};

[[nodiscard]] std::shared_ptr<IBooleanEvaluator> MakeCompositeBooleanEvaluator();

}  // namespace brep::boolean
