#include "brep/bool/evaluator.hpp"

#include "brep/log.hpp"

#include <memory>
#include <string>

namespace brep::boolean {
namespace {

BooleanEvaluatorFactory& evaluator_factory() {
  static BooleanEvaluatorFactory factory;
  return factory;
}

[[nodiscard]] const char* op_name(BooleanOp op) noexcept {
  switch (op) {
    case BooleanOp::Union:
      return "Union";
    case BooleanOp::Subtract:
      return "Subtract";
    case BooleanOp::Intersect:
      return "Intersect";
  }
  return "Unknown";
}

class StubBooleanEvaluator final : public IBooleanEvaluator {
 public:
  BooleanResult evaluate(BooleanOp op, Model& /*model*/, const Body& a,
                         const Body& b,
                         const BooleanContext& /*ctx*/) override {
    BooleanResult result;
    result.mode = BooleanEvalMode::General;
    result.diagnostics =
        std::string("boolean: unsupported combination for ") + op_name(op) +
        " ('" + a.name + "' vs '" + b.name +
        "'); evaluator stub — box/general paths not implemented yet";
    BREP_WARN("{}", result.diagnostics);
    return result;
  }
};

}  // namespace

std::unique_ptr<IBooleanEvaluator> make_stub_boolean_evaluator() {
  return std::make_unique<StubBooleanEvaluator>();
}

void set_boolean_evaluator_factory(BooleanEvaluatorFactory factory) {
  evaluator_factory() = std::move(factory);
}

std::shared_ptr<IBooleanEvaluator> make_default_boolean_evaluator() {
  if (evaluator_factory()) {
    return evaluator_factory()();
  }
  return make_stub_boolean_evaluator();
}

}  // namespace brep::boolean
