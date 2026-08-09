#include "ecs/world.hpp"

#include "adapter/scene_adapter.hpp"
#include "ecs/systems.hpp"

#include "api/core.hpp"
#include "api/modeling.hpp"

#include <vector>

namespace brep::viewer::ecs {

World::World() {
  registry_.ctx().emplace<InputState>();
  registry_.ctx().emplace<SelectionState>();
  registry_.ctx().emplace<RenderCache>();
}

brep::Model* World::model() noexcept {
  if (!document_) return nullptr;
  if (auto* part = document_->main_part()) return &part->model();
  return nullptr;
}

const brep::Model* World::model() const noexcept {
  if (!document_) return nullptr;
  if (const auto* part = document_->main_part()) return &part->model();
  return nullptr;
}

void World::clear_scene() {
  registry_.clear();
  if (!registry_.ctx().contains<InputState>()) {
    registry_.ctx().emplace<InputState>();
  } else {
    registry_.ctx().get<InputState>() = InputState{};
  }
  if (!registry_.ctx().contains<SelectionState>()) {
    registry_.ctx().emplace<SelectionState>();
  } else {
    registry_.ctx().get<SelectionState>() = SelectionState{};
  }
  if (!registry_.ctx().contains<RenderCache>()) {
    registry_.ctx().emplace<RenderCache>();
  } else {
    registry_.ctx().get<RenderCache>() = RenderCache{};
  }
  document_.reset();
}

entt::entity World::create_camera(Camera camera) {
  const entt::entity e = registry_.create();
  registry_.emplace<Name>(e, Name{"main_camera"});
  registry_.emplace<CameraComponent>(e, CameraComponent{std::move(camera)});
  registry_.emplace<MainCameraTag>(e);
  return e;
}

entt::entity World::create_renderable(std::string name, TriangleMesh triangles,
                                      EdgeMesh edges, Material material,
                                      Point3d position) {
  const entt::entity e = registry_.create();
  registry_.emplace<Name>(e, Name{std::move(name)});
  registry_.emplace<Transform>(e, Transform{position});
  registry_.emplace<MeshComponent>(
      e, MeshComponent{std::move(triangles), std::move(edges), true});
  registry_.emplace<MaterialComponent>(
      e, MaterialComponent{std::move(material), true});
  registry_.emplace<RenderableTag>(e);
  return e;
}

entt::entity World::create_body_renderable(std::string name,
                                           brep::Guid body_guid,
                                           TriangleMesh triangles,
                                           EdgeMesh edges, Material material,
                                           Point3d position,
                                           brep::Guid feature_guid) {
  const entt::entity e = create_renderable(
      std::move(name), std::move(triangles), std::move(edges),
      std::move(material), position);
  registry_.emplace<BodyRef>(e, BodyRef{body_guid});
  if (!feature_guid.is_nil()) {
    registry_.emplace<FeatureRef>(e, FeatureRef{feature_guid});
  }
  return e;
}

bool World::update_body_renderable(const brep::Guid& body_guid,
                                   TriangleMesh triangles, EdgeMesh edges) {
  const entt::entity e = find_body_renderable(body_guid);
  if (e == entt::null) return false;
  auto& mesh = registry_.get<MeshComponent>(e);
  mesh.triangles = std::move(triangles);
  mesh.edges = std::move(edges);
  mesh.dirty = true;
  if (auto* cache = registry_.ctx().find<RenderCache>()) {
    cache->force_rebuild = true;
  }
  return true;
}

void World::sync_part_bodies(brep::Part& part, Material material,
                             const brep::io::BodyMeshCache* cache) {
  adapter::SceneAdapter scene(document_.get());
  std::vector<Guid> live;
  for (const auto& body : part.model().bodies()) {
    if (!body) continue;
    live.push_back(body->guid);
    Guid feature_guid{};
    if (const auto* f = part.features().find_by_body(body->guid)) {
      feature_guid = f->id().guid;
    }

    auto mesh = scene.mesh_for_body(body->guid, cache);

    const entt::entity existing = find_body_renderable(body->guid);
    if (existing == entt::null) {
      create_body_renderable(body->name, body->guid, std::move(mesh.faces),
                             std::move(mesh.edges), material, Point3d{},
                             feature_guid);
    } else {
      update_body_renderable(body->guid, std::move(mesh.faces),
                             std::move(mesh.edges));
      if (!feature_guid.is_nil()) {
        if (registry_.all_of<FeatureRef>(existing)) {
          registry_.get<FeatureRef>(existing).feature_guid = feature_guid;
        } else {
          registry_.emplace<FeatureRef>(existing, FeatureRef{feature_guid});
        }
      }
    }
  }

  std::vector<entt::entity> stale;
  auto view = registry_.view<BodyRef, RenderableTag>();
  for (auto entity : view) {
    const Guid g = view.get<BodyRef>(entity).guid;
    bool found = false;
    for (const auto& live_g : live) {
      if (live_g == g) {
        found = true;
        break;
      }
    }
    if (!found) stale.push_back(entity);
  }
  for (auto entity : stale) {
    if (registry_.all_of<SelectedTag>(entity)) {
      toggle_selection(registry_, entity);
    }
    registry_.destroy(entity);
  }
}

entt::entity World::find_body_renderable(const brep::Guid& body_guid) const {
  auto view = registry_.view<BodyRef, RenderableTag>();
  for (auto entity : view) {
    if (view.get<BodyRef>(entity).guid == body_guid) return entity;
  }
  return entt::null;
}

bool World::destroy_body_renderable(const brep::Guid& body_guid) {
  const entt::entity e = find_body_renderable(body_guid);
  if (e == entt::null) return false;
  if (registry_.all_of<SelectedTag>(e)) {
    toggle_selection(registry_, e);
  }
  registry_.destroy(e);
  return true;
}

void World::create_blank_scene() {
  clear_scene();

  document_ = adapter::SceneAdapter::create_blank("Untitled");
  Part* part = document_->main_part();

  Camera cam;
  cam.target = Point3d{0.0, 0.0, 0.0};
  cam.distance = 6.0f;
  create_camera(cam);

  BREP_INFO("blank scene: Document={} Part={} (no bodies)",
            document_->guid.to_string(),
            part ? part->guid.to_string() : std::string{});
}

void World::create_demo_box_scene(const std::string& wood_albedo_path) {
  clear_scene();

  document_ = adapter::SceneAdapter::create_blank("Untitled");
  adapter::SceneAdapter scene(document_.get());
  Body* body = scene.add_box(BoxSpec{
      .min = Point3d{0, 0, 0},
      .max = Point3d{2, 1, 3},
      .name = "demo_box",
  });

  Camera cam;
  cam.target = Point3d{1.0, 0.5, 1.5};
  create_camera(cam);

  Guid feature_guid{};
  if (body) {
    if (auto obj = scene.object_for_body(body->guid)) {
      feature_guid = obj->feature_guid;
    }
    auto mesh = scene.mesh_for_body(body->guid);
    create_body_renderable("demo_box", body->guid, std::move(mesh.faces),
                           std::move(mesh.edges),
                           make_wood_material(wood_albedo_path), Point3d{},
                           feature_guid);
  }

  BREP_INFO(
      "demo scene: Document={} Part={} Body={} (guid={})",
      document_->guid.to_string(),
      document_->main_part() ? document_->main_part()->guid.to_string()
                             : std::string{},
      body ? body->name : std::string{},
      body ? body->guid.to_string() : std::string{});
}

void World::adopt_document(std::unique_ptr<brep::Document> document,
                           Material material,
                           const brep::io::BodyMeshCache* cache) {
  if (!document) return;

  // Keep InputState / Selection / RenderCache; drop entities only.
  registry_.clear();
  if (!registry_.ctx().contains<InputState>()) {
    registry_.ctx().emplace<InputState>();
  } else {
    registry_.ctx().get<InputState>() = InputState{};
  }
  if (!registry_.ctx().contains<SelectionState>()) {
    registry_.ctx().emplace<SelectionState>();
  } else {
    registry_.ctx().get<SelectionState>() = SelectionState{};
  }
  if (!registry_.ctx().contains<RenderCache>()) {
    registry_.ctx().emplace<RenderCache>();
  } else {
    registry_.ctx().get<RenderCache>() = RenderCache{};
  }

  document_ = std::move(document);

  Camera cam;
  cam.target = Point3d{0.0, 0.0, 0.0};
  cam.distance = 6.0f;
  if (Part* part = document_->main_part()) {
    // Frame roughly around existing bodies.
    if (!part->model().bodies().empty() && part->model().bodies().front()) {
      cam.target = Point3d{1.0, 0.5, 1.5};
    }
    create_camera(cam);
    sync_part_bodies(*part, std::move(material), cache);
  } else {
    create_camera(cam);
  }

  BREP_INFO("adopted Document={} parts={} occurrences={}",
            document_->guid.to_string(), document_->parts().size(),
            document_->assembly().occurrences().size());
}

Camera* World::main_camera() noexcept {
  auto view = registry_.view<CameraComponent, MainCameraTag>();
  for (auto entity : view) {
    return &view.get<CameraComponent>(entity).camera;
  }
  return nullptr;
}

const Camera* World::main_camera() const noexcept {
  auto view = registry_.view<CameraComponent, MainCameraTag>();
  for (auto entity : view) {
    return &view.get<CameraComponent>(entity).camera;
  }
  return nullptr;
}

}  // namespace brep::viewer::ecs
