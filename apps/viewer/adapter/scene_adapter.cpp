#include "adapter/scene_adapter.hpp"

#include "api/mesh.hpp"
#include "api/modeling.hpp"

#include <utility>

namespace brep::viewer::adapter {

brep::Part* SceneAdapter::main_part() const noexcept {
  return document_ ? document_->main_part() : nullptr;
}

const brep::Part* SceneAdapter::main_part_const() const noexcept {
  return document_ ? document_->main_part() : nullptr;
}

MeshBundle SceneAdapter::mesh_for_body(
    const Guid& body_guid, const brep::io::BodyMeshCache* cache) const {
  MeshBundle out;
  const Part* part = main_part_const();
  if (!part) return out;

  if (cache && cache->has(body_guid)) {
    out.faces = cache->triangles.at(body_guid);
    out.edges = cache->edges.at(body_guid);
    return out;
  }

  const Body* body = part->find_body(body_guid);
  if (!body) return out;
  out.faces = tessellate_body(*body);
  out.edges = extract_edges(*body);
  return out;
}

const feat::IFeature* SceneAdapter::find_box_feature(
    feat::FeatureId id) const {
  const Part* part = main_part_const();
  if (!part || id.is_nil()) return nullptr;
  const auto* f = part->features().find(id);
  if (!f || f->type_name() != "Box") return nullptr;
  return f;
}

std::optional<BoxParams> SceneAdapter::box_params_from_feature(
    const feat::IFeature& feature) const {
  const Part* part = main_part_const();
  if (!part || feature.type_name() != "Box") return std::nullopt;
  const auto& box = static_cast<const feat::BoxFeature&>(feature);
  const auto& params = part->parameters();
  return BoxParams{
      .length = params.get(box.length_id()).value_or(0.0),
      .width = params.get(box.width_id()).value_or(0.0),
      .height = params.get(box.height_id()).value_or(0.0),
  };
}

std::optional<SceneObject> SceneAdapter::object_for_feature(
    feat::FeatureId id) const {
  const Part* part = main_part_const();
  if (!part || id.is_nil()) return std::nullopt;
  const auto* f = part->features().find(id);
  if (!f) return std::nullopt;

  SceneObject obj;
  obj.feature_guid = f->id().guid;
  obj.body_guid = f->body_guid();
  obj.name = std::string(f->display_name());
  obj.type_name = std::string(f->type_name());
  if (f->type_name() == "Box") {
    obj.box = box_params_from_feature(*f);
  }
  return obj;
}

std::optional<SceneObject> SceneAdapter::object_for_body(
    const Guid& body_guid) const {
  const Part* part = main_part_const();
  if (!part) return std::nullopt;
  const auto* f = part->features().find_by_body(body_guid);
  if (!f) {
    const Body* body = part->find_body(body_guid);
    if (!body) return std::nullopt;
    SceneObject obj;
    obj.body_guid = body_guid;
    obj.name = body->name;
    obj.type_name = "Body";
    return obj;
  }
  return object_for_feature(f->id());
}

std::optional<BoxParams> SceneAdapter::box_params(feat::FeatureId id) const {
  const auto* f = find_box_feature(id);
  if (!f) return std::nullopt;
  return box_params_from_feature(*f);
}

bool SceneAdapter::set_box_params(feat::FeatureId id, const BoxParams& params) {
  Part* part = main_part();
  if (!part || !find_box_feature(id)) return false;
  return part->edit_feature_params(id, {{"Length", params.length},
                                        {"Width", params.width},
                                        {"Height", params.height}});
}

Body* SceneAdapter::add_box(const BoxSpec& spec) {
  Part* part = main_part();
  if (!part) return nullptr;
  return part->add_box(spec);
}

void SceneAdapter::record_append_feature(feat::FeatureId id, BoxSpec undo_spec) {
  Part* part = main_part();
  if (!part || id.is_nil()) return;
  feat::FeatureTransaction tx;
  tx.kind = feat::TxKind::AppendFeature;
  tx.feature = id;
  tx.feature_type = "Box";
  tx.box_spec = std::move(undo_spec);
  part->feature_history().record(std::move(tx));
}

std::optional<feat::FeatureId> SceneAdapter::feature_id_for(
    Guid feature_guid, Guid body_guid) const {
  const Part* part = main_part_const();
  if (!part) return std::nullopt;
  const feat::IFeature* f = nullptr;
  if (!feature_guid.is_nil()) {
    f = part->features().find(feat::FeatureId{feature_guid});
  }
  if (!f && !body_guid.is_nil()) {
    f = part->features().find_by_body(body_guid);
  }
  if (!f) return std::nullopt;
  return f->id();
}

bool SceneAdapter::remove_feature(feat::FeatureId id) {
  Part* part = main_part();
  if (!part || id.is_nil()) return false;
  feat::IFeature* f = part->features().find(id);
  if (!f) return false;

  feat::FeatureTransaction tx;
  tx.kind = feat::TxKind::RemoveFeature;
  tx.feature = id;
  tx.feature_type = std::string(f->type_name());
  tx.sketch_name = f->display_name();

  if (tx.feature_type == "Box") {
    const auto& box = static_cast<const feat::BoxFeature&>(*f);
    tx.box_spec = box.to_spec(part->parameters());
    tx.box_origin = box.origin();
  } else if (tx.feature_type == "Extrude") {
    const auto& ext = static_cast<const feat::ExtrudeFeature&>(*f);
    tx.sketch_feature = ext.sketch_feature_id();
    tx.extrude_distance =
        part->parameters().get(ext.distance_id()).value_or(1.0);
  }

  part->feature_history().apply_and_record(*part, std::move(tx));
  return true;
}

void SceneAdapter::undo_feature(int steps) {
  Part* part = main_part();
  if (!part) return;
  for (int i = 0; i < steps; ++i) {
    if (!part->feature_history().undo(*part)) break;
  }
}

void SceneAdapter::redo_feature(int steps) {
  Part* part = main_part();
  if (!part) return;
  for (int i = 0; i < steps; ++i) {
    if (!part->feature_history().redo(*part)) break;
  }
}

std::optional<BoxSpec> SceneAdapter::box_spec_for(Guid feature_guid,
                                                  Guid body_guid) const {
  const Part* part = main_part_const();
  if (!part) return std::nullopt;

  const feat::IFeature* f = nullptr;
  if (!feature_guid.is_nil()) {
    f = part->features().find(feat::FeatureId{feature_guid});
  }
  if (!f && !body_guid.is_nil()) {
    f = part->features().find_by_body(body_guid);
  }
  if (!f || f->type_name() != "Box") return std::nullopt;
  const auto& box = static_cast<const feat::BoxFeature&>(*f);
  return box.to_spec(part->parameters());
}

std::unique_ptr<brep::Document> SceneAdapter::create_blank(
    const std::string& name) {
  auto doc = Document::create(name);
  doc->add_part("MainPart");
  return doc;
}

}  // namespace brep::viewer::adapter
