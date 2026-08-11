#pragma once

#include "brep/bool/context.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
#include "brep/model.hpp"
#include "brep/topology.hpp"

#include <functional>
#include <memory>

namespace brep::boolean {

/// Boolean evaluation backend (OCCT-inspired pipeline; self-hosted).
class IBooleanEvaluator {
 public:
  virtual ~IBooleanEvaluator() = default;

  [[nodiscard]] virtual BooleanResult evaluate(BooleanOp op, Model& model,
                                               const Body& a, const Body& b,
                                               const BooleanContext& ctx) = 0;
};

/// Phase 2.0.1 skeleton stub (always fails). Prefer
/// `make_default_boolean_evaluator` for production (box–box path).
[[nodiscard]] std::unique_ptr<IBooleanEvaluator> make_stub_boolean_evaluator();

using BooleanEvaluatorFactory =
    std::function<std::shared_ptr<IBooleanEvaluator>()>;

/// Optional override used by Part / XL load (tests inject a fake until needed).
void set_boolean_evaluator_factory(BooleanEvaluatorFactory factory);
/// Default: axis-aligned box–box Fuse/Cut/Common; other combos fail soft.
[[nodiscard]] std::shared_ptr<IBooleanEvaluator> make_default_boolean_evaluator();

}  // namespace brep::boolean
