#include "brep/feat/boolean_feature.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/builder.hpp"
#include "brep/feat/box_feature.hpp"
#include "brep/feat/sphere_feature.hpp"
#include "brep/guid.hpp"
#include "brep/log.hpp"
#include "brep/part.hpp"

namespace brep::feat {
namespace {

Body* materialize_operand(Part& part, IFeature& feature,
                          param::ParameterStore& params, Model& scratch) {
  if (Body* live = part.find_body(feature.body_guid())) {
    return live;
  }

  if (feature.type_name() == "Box") {
    auto& box = static_cast<BoxFeature&>(feature);
    return make_box(scratch, box.to_spec(params));
  }
  if (feature.type_name() == "Sphere") {
    auto& sphere = static_cast<SphereFeature&>(feature);
    return make_sphere(scratch, sphere.to_spec(params));
  }

  BREP_WARN("BooleanFeature: cannot materialize operand type '{}'",
            feature.type_name());
  return nullptr;
}

void suppress_operand_body(Part& part, IFeature& feature) {
  feature.set_suppressed(true);
  feature.set_status(FeatureStatus::Suppressed);
  const Guid guid = feature.body_guid();
  if (guid.is_nil()) return;
  if (!part.find_body(guid)) return;
  part.unregister_body(guid);
  part.model().remove_body(guid);
}

}  // namespace

BooleanFeature::BooleanFeature(FeatureId id, std::string name,
                               boolean::BooleanOp op, FeatureId target,
                               FeatureId tool)
    : id_(id),
      name_(std::move(name)),
      op_(op),
      target_(target),
      tool_(tool) {}

std::unique_ptr<BooleanFeature> BooleanFeature::create(boolean::BooleanOp op,
                                                       FeatureId target,
                                                       FeatureId tool,
                                                       std::string name) {
  FeatureId fid{Guid::generate()};
  if (name.empty()) name = "Boolean";
  return std::make_unique<BooleanFeature>(fid, std::move(name), op, target,
                                          tool);
}

void BooleanFeature::collect_parameters(param::ParameterStore& /*store*/) {}

bool BooleanFeature::rebuild(Part& part, param::ParameterStore& params) {
  IFeature* target_f = part.features().find(target_);
  IFeature* tool_f = part.features().find(tool_);
  if (!target_f || !tool_f) {
    BREP_ERROR("BooleanFeature '{}': missing target/tool feature", name_);
    return false;
  }
  if (target_f == tool_f || target_ == tool_) {
    BREP_ERROR("BooleanFeature '{}': target and tool must differ", name_);
    return false;
  }

  Model scratch;
  Body* body_a = materialize_operand(part, *target_f, params, scratch);
  Body* body_b = materialize_operand(part, *tool_f, params, scratch);
  if (!body_a || !body_b) {
    BREP_ERROR("BooleanFeature '{}': failed to resolve operand bodies", name_);
    return false;
  }

  const boolean::BooleanResult eval = part.boolean_evaluator().evaluate(
      op_, part.model(), *body_a, *body_b, boolean::BooleanContext{});
  if (!eval.ok()) {
    BREP_WARN("BooleanFeature '{}': evaluate failed: {}", name_,
              eval.diagnostics);
    return false;
  }

  Body* out = part.rebuild_boolean_body(body_guid_, eval.body);
  if (!out) return false;
  body_guid_ = out->guid;

  suppress_operand_body(part, *target_f);
  suppress_operand_body(part, *tool_f);
  return true;
}

}  // namespace brep::feat
