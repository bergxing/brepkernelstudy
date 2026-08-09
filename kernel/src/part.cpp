#include "brep/part.hpp"

#include "brep/document.hpp"
#include "brep/feat/box_feature.hpp"
#include "brep/feat/extrude_feature.hpp"
#include "brep/feat/sketch_feature.hpp"
#include "brep/log.hpp"
#include "brep/ops/profile.hpp"

namespace brep {

Part::Part(std::string part_name) : IObject(std::move(part_name)) {}

Body* Part::find_body(const Guid& guid) {
  for (const auto& body : model_.bodies()) {
    if (body && body->guid == guid) return body.get();
  }
  return nullptr;
}

const Body* Part::find_body(const Guid& guid) const {
  for (const auto& body : model_.bodies()) {
    if (body && body->guid == guid) return body.get();
  }
  return nullptr;
}

void Part::register_body(Body& body) {
  if (!document_) {
    BREP_WARN("Part::register_body: part '{}' has no Document; Guid {} not "
              "registered",
              name, body.guid.to_string());
    return;
  }
  document_->registry().add(body);
  document_->mark_dirty();
  BREP_INFO("registered Body '{}' guid={} on Document '{}'", body.name,
            body.guid.to_string(), document_->name);
}

void Part::unregister_body(const Guid& guid) {
  if (document_) document_->registry().remove(guid);
}

Body* Part::rebuild_box_body(Guid keep_guid, const BoxSpec& spec) {
  if (!keep_guid.is_nil()) {
    unregister_body(keep_guid);
    model_.remove_body(keep_guid);
  }
  Body* body = make_box(model_, spec);
  if (!keep_guid.is_nil()) body->guid = keep_guid;
  register_body(*body);
  return body;
}

Body* Part::rebuild_extrude_body(Guid keep_guid, const ops::ExtrudeSpec& spec) {
  if (!keep_guid.is_nil()) {
    unregister_body(keep_guid);
    model_.remove_body(keep_guid);
  }
  Body* body = ops::extrude(model_, spec);
  if (!body) return nullptr;
  if (!keep_guid.is_nil()) body->guid = keep_guid;
  register_body(*body);
  return body;
}

feat::RegenResult Part::regenerate() {
  return feat::Regenerator::run(*this, features_, params_);
}

Body* Part::add_box(const BoxSpec& spec) {
  auto feature = feat::BoxFeature::create(params_, spec);
  const feat::FeatureId fid = features_.append(std::move(feature));
  const auto result = regenerate();
  if (!result.ok) {
    BREP_WARN("Part::add_box regenerate failed: {}", result.message);
    return nullptr;
  }
  auto* f = features_.find(fid);
  if (!f) return nullptr;
  return find_body(f->body_guid());
}

feat::FeatureId Part::add_rectangle_sketch(std::string name, Point2d min,
                                           Point2d max) {
  auto feature =
      feat::SketchFeature::create_rectangle(params_, std::move(name), min, max);
  const feat::FeatureId fid = features_.append(std::move(feature));
  regenerate();
  return fid;
}

Body* Part::add_extrude(feat::FeatureId sketch_feature, double distance,
                        std::string name) {
  auto feature = feat::ExtrudeFeature::create(params_, std::move(name),
                                              sketch_feature, distance);
  const feat::FeatureId fid = features_.append(std::move(feature));
  const auto result = regenerate();
  if (!result.ok) return nullptr;
  auto* f = features_.find(fid);
  if (!f) return nullptr;
  return find_body(f->body_guid());
}

bool Part::remove_feature(feat::FeatureId id) {
  feat::IFeature* f = features_.find(id);
  if (!f) return false;
  const Guid body = f->body_guid();
  if (!body.is_nil()) {
    unregister_body(body);
    model_.remove_body(body);
  }
  // Drop parameters owned exclusively by Box/Extrude/Sketch — best-effort by
  // name prefix is skipped; leave params in store for history redo.
  features_.remove(id);
  features_.mark_all_dirty();
  regenerate();
  if (document_) document_->mark_dirty();
  return true;
}

bool Part::edit_feature_params(
    feat::FeatureId id,
    std::initializer_list<std::pair<std::string_view, double>> named_vals) {
  auto* feature = features_.find(id);
  if (!feature) return false;

  feat::FeatureTransaction tx;
  tx.kind = feat::TxKind::EditParameters;
  tx.feature = id;
  tx.feature_type = std::string(feature->type_name());

  if (feature->type_name() == "Box") {
    auto* box = static_cast<feat::BoxFeature*>(feature);
    const param::ParameterId ids[3] = {box->length_id(), box->width_id(),
                                       box->height_id()};
    const char* keys[3] = {"Length", "Width", "Height"};
    for (const auto& [key, value] : named_vals) {
      for (int i = 0; i < 3; ++i) {
        if (key == keys[i]) {
          tx.param_before.emplace_back(ids[i],
                                       params_.get(ids[i]).value_or(0.0));
          tx.param_after.emplace_back(ids[i], value);
        }
      }
    }
  } else {
    for (const auto& [key, value] : named_vals) {
      if (auto* p = params_.find_by_name(std::string(feature->display_name()) +
                                         "." + std::string(key))) {
        tx.param_before.emplace_back(p->id, p->value);
        tx.param_after.emplace_back(p->id, value);
      }
    }
  }

  if (tx.param_after.empty()) return false;
  history_.apply_and_record(*this, std::move(tx));
  return true;
}

}  // namespace brep
