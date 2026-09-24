#include "brep/bool/CompositeEvaluator.h"

#include "brep/bool/Broadphase.h"
#include "brep/bool/Pipeline.h"
#include "brep/Log.h"

#include <memory>
#include <string>

namespace brep::boolean
{
namespace
{

[[nodiscard]] const char* OpName(BooleanOp op) noexcept
{
  switch (op)
  {
  case BooleanOp::Union:
    return "Union";
  case BooleanOp::Subtract:
    return "Subtract";
  case BooleanOp::Intersect:
    return "Intersect";
  }
  return "Unknown";
}

}  // namespace

PipelineBooleanEvaluator::PipelineBooleanEvaluator(
    std::unique_ptr<BooleanPipeline> pipeline)
    : m_pipeline(std::move(pipeline))
{
}

BooleanResult PipelineBooleanEvaluator::Evaluate(BooleanOp op, Model& model,
                                                 const Body& a, const Body& b,
                                                 const BooleanContext& ctx)
{
  if (m_pipeline)
  {
    BooleanResult piped = m_pipeline->Evaluate(op, model, a, b, ctx);
    if (piped.Ok() || !piped.Diagnostics.empty())
    {
      return piped;
    }
  }

  const BroadphaseProbe probe =
      ProbeFacePairIntersections(a, b, spatial::BuildQuality::Sah, ctx);
  BooleanResult result;
  result.Mode = BooleanEvalMode::General;
  result.Diagnostics = std::string("boolean: pipeline failed for ") + OpName(op) +
                       " ('" + a.Name + "' vs '" + b.Name + "'); " + probe.Summary;
  BREP_WARN("{}", result.Diagnostics);
  return result;
}

std::shared_ptr<IBooleanEvaluator> MakePipelineBooleanEvaluator()
{
  return std::make_shared<PipelineBooleanEvaluator>(MakeDefaultBooleanPipeline());
}

std::shared_ptr<IBooleanEvaluator> MakeCompositeBooleanEvaluator()
{
  return MakePipelineBooleanEvaluator();
}

}  // namespace brep::boolean
