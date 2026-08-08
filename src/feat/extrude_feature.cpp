#include "brep/feat/extrude_feature.hpp"

#include "brep/ops/profile.hpp"
#include "brep/part.hpp"

namespace brep::feat {

ExtrudeFeature::ExtrudeFeature(FeatureId id, std::string name,
                               FeatureId sketch_feature,
                               param::ParameterId distance)
    : id_(id),
      name_(std::move(name)),
      sketch_feature_(sketch_feature),
      distance_(distance) {}

std::unique_ptr<ExtrudeFeature> ExtrudeFeature::create(
    param::ParameterStore& store, std::string name, FeatureId sketch_feature,
    double distance) {
  auto dist = store.add(name + ".Depth", param::ParamKind::Length, distance);
  FeatureId id{Guid::generate()};
  return std::make_unique<ExtrudeFeature>(id, std::move(name), sketch_feature,
                                          dist);
}

void ExtrudeFeature::collect_parameters(param::ParameterStore& /*store*/) {}

bool ExtrudeFeature::rebuild(Part& part, param::ParameterStore& params) {
  auto* base = part.features().find(sketch_feature_);
  if (!base || base->type_name() != "Sketch") return false;
  auto* sketch_feat = static_cast<SketchFeature*>(base);

  ops::ExtrudeSpec spec;
  spec.profile = ops::extract_profile(sketch_feat->sketch());
  spec.plane = sketch_feat->sketch().frame();
  spec.distance = params.get(distance_).value_or(1.0);
  spec.name = name_;

  Body* body = part.rebuild_extrude_body(body_guid_, spec);
  if (!body) return false;
  body_guid_ = body->guid;
  return true;
}

}  // namespace brep::feat
