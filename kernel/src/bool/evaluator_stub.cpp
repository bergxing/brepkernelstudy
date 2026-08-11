#include "brep/bool/evaluator.hpp"

#include "brep/bool/box_boolean.hpp"
#include "brep/bool/box_recognize.hpp"
#include "brep/bool/broadphase.hpp"
#include "brep/bool/planar_boolean.hpp"
#include "brep/bool/planar_recognize.hpp"
#include "brep/bool/sphere_box_boolean.hpp"
#include "brep/bool/sphere_recognize.hpp"
#include "brep/bool/sphere_sphere_boolean.hpp"
#include "brep/log.hpp"
#include "brep/spatial/face_bvh.hpp"

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

class DefaultBooleanEvaluator final : public IBooleanEvaluator {
 public:
  BooleanResult evaluate(BooleanOp op, Model& model, const Body& a,
                         const Body& b, const BooleanContext& ctx) override {
    if (recognize_axis_aligned_box(a, ctx) &&
        recognize_axis_aligned_box(b, ctx)) {
      return evaluate_box_boolean(op, model, a, b, ctx);
    }

    const auto prism_a = recognize_extrusion_prism(a, ctx);
    const auto prism_b = recognize_extrusion_prism(b, ctx);
    const auto box_a = recognize_axis_aligned_box(a, ctx);
    const auto box_b = recognize_axis_aligned_box(b, ctx);
    const auto sphere_a = recognize_analytic_sphere(a, ctx);
    const auto sphere_b = recognize_analytic_sphere(b, ctx);

    if (prism_a && box_b) {
      return evaluate_prism_box_boolean(op, model, *prism_a, *box_b,
                                        /*prism_is_a=*/true, ctx);
    }
    if (box_a && prism_b) {
      return evaluate_prism_box_boolean(op, model, *prism_b, *box_a,
                                        /*prism_is_a=*/false, ctx);
    }
    if (sphere_a && box_b) {
      return evaluate_sphere_box_boolean(op, model, *sphere_a, *box_b,
                                         /*sphere_is_a=*/true, ctx);
    }
    if (box_a && sphere_b) {
      return evaluate_sphere_box_boolean(op, model, *sphere_b, *box_a,
                                         /*sphere_is_a=*/false, ctx);
    }
    if (sphere_a && sphere_b) {
      return evaluate_sphere_sphere_boolean(op, model, *sphere_a, *sphere_b,
                                            ctx);
    }

    BooleanResult result;
    result.mode = BooleanEvalMode::General;
    // Existing specialized paths (box / prism / ⅛-ball / Sphere−Box / Sphere∪Sphere)
    // Soft-fail hook: Sah broad-phase + analytic intersect probe for diagnostics.
    const BroadphaseProbe probe =
        probe_face_pair_intersections(a, b, spatial::BuildQuality::Sah, ctx);
    result.diagnostics =
        std::string("boolean: unsupported combination for ") + op_name(op) +
        " ('" + a.name + "' vs '" + b.name +
        "'); supported: box–box, prism–box, sphere–box (Intersect ⅛ / "
        "Sphere−Box), sphere–sphere (Union); " +
        probe.summary;
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
  return std::make_shared<DefaultBooleanEvaluator>();
}

}  // namespace brep::boolean
