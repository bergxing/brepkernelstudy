#pragma once

#include "brep/bool/context.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
#include "brep/model.hpp"
#include "brep/topology.hpp"

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

/// Phase 2.0.1 skeleton: links and always fails with a clear diagnostic until
/// box / general paths are implemented (T2.0.4+).
[[nodiscard]] std::unique_ptr<IBooleanEvaluator> make_stub_boolean_evaluator();

}  // namespace brep::boolean
