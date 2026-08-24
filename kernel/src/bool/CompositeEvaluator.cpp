#include "brep/bool/CompositeEvaluator.h"

#include "brep/bool/Broadphase.h"
#include "brep/bool/FastPath.h"
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

CompositeBooleanEvaluator::CompositeBooleanEvaluator(
    AnalyticFastPathRegistry fastPaths, std::unique_ptr<BooleanPipeline> pipeline)
    : m_fastPaths(std::move(fastPaths)), m_pipeline(std::move(pipeline))
{
}

BooleanResult CompositeBooleanEvaluator::Evaluate(BooleanOp op, Model& model,
                                                  const Body& a, const Body& b,
                                                  const BooleanContext& ctx)
{
  for (const auto& path : m_fastPaths.Paths())
  {
    if (!path->CanHandle(op, a, b, ctx))
    {
      continue;
    }
    return path->Evaluate(op, model, a, b, ctx);
  }

  if (m_pipeline)
  {
    BooleanResult piped = m_pipeline->Evaluate(op, model, a, b, ctx);
    if (piped.Ok())
    {
      return piped;
    }
    if (!piped.Diagnostics.empty())
    {
      return piped;
    }
  }

  const BroadphaseProbe probe =
      ProbeFacePairIntersections(a, b, spatial::BuildQuality::Sah, ctx);
  BooleanResult result;
  result.Mode = BooleanEvalMode::General;
  result.Diagnostics = std::string("boolean: no fast path and pipeline failed for ") +
                       OpName(op) + " ('" + a.Name + "' vs '" + b.Name + "'); " +
                       probe.Summary;
  BREP_WARN("{}", result.Diagnostics);
  return result;
}

std::shared_ptr<IBooleanEvaluator> MakeCompositeBooleanEvaluator()
{
  return std::make_shared<CompositeBooleanEvaluator>(MakeDefaultFastPathRegistry(),
                                                     MakeDefaultBooleanPipeline());
}

}  // namespace brep::boolean
