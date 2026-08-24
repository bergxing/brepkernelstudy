#include "brep/bool/Evaluator.h"

#include "brep/bool/CompositeEvaluator.h"
#include "brep/Log.h"

#include <memory>
#include <string>

namespace brep::boolean
{
namespace
{

BooleanEvaluatorFactory& EvaluatorFactory()
{
    static BooleanEvaluatorFactory factory;
    return factory;
}

class StubBooleanEvaluator final : public IBooleanEvaluator
{
public:
    BooleanResult Evaluate(BooleanOp op, Model& /*model*/, const Body& a, const Body& b,
                           const BooleanContext& /*ctx*/) override
    {
        BooleanResult result;
        result.Mode = BooleanEvalMode::General;
        const char* opStr = "Unknown";
        switch (op)
        {
        case BooleanOp::Union:
            opStr = "Union";
            break;
        case BooleanOp::Subtract:
            opStr = "Subtract";
            break;
        case BooleanOp::Intersect:
            opStr = "Intersect";
            break;
        }
        result.Diagnostics =
            std::string("boolean: unsupported combination for ") + opStr + " ('" +
            a.Name + "' vs '" + b.Name +
            "'); evaluator stub — use MakeDefaultBooleanEvaluator()";
        BREP_WARN("{}", result.Diagnostics);
        return result;
    }
};

}  // namespace

std::unique_ptr<IBooleanEvaluator> MakeStubBooleanEvaluator()
{
    return std::make_unique<StubBooleanEvaluator>();
}

void SetBooleanEvaluatorFactory(BooleanEvaluatorFactory factory)
{
    EvaluatorFactory() = std::move(factory);
}

std::shared_ptr<IBooleanEvaluator> MakeDefaultBooleanEvaluator()
{
  if (EvaluatorFactory())
  {
    return EvaluatorFactory()();
  }
  return MakeCompositeBooleanEvaluator();
}

}  // namespace brep::boolean
