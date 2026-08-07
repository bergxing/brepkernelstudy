#include "ecs/world.hpp"

#include "ecs/systems.hpp"

#include "brep/brep.hpp"

namespace brep::viewer::ecs {

World::World() {
  registry_.ctx().emplace<InputState>();
  registry_.ctx().emplace<SelectionState>();
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
                                           Point3d position) {
  const entt::entity e = create_renderable(
      std::move(name), std::move(triangles), std::move(edges),
      std::move(material), position);
  registry_.emplace<BodyRef>(e, BodyRef{body_guid});
  return e;
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
  if (selected_entity(registry_) == e) {
    clear_selection(registry_);
  }
  registry_.destroy(e);
  return true;
}

void World::create_blank_scene() {
  using namespace brep;

  clear_scene();

  document_ = Document::create("Untitled");
  Part& part = document_->add_part("MainPart");

  Camera cam;
  cam.target = Point3d{0.0, 0.0, 0.0};
  cam.distance = 6.0f;
  create_camera(cam);

  BREP_INFO("blank scene: Document={} Part={} (no bodies)",
            document_->guid.to_string(), part.guid.to_string());
}

void World::create_demo_box_scene(const std::string& wood_albedo_path) {
  using namespace brep;

  clear_scene();

  document_ = Document::create("Untitled");
  Part& part = document_->add_part("MainPart");
  Body* body = part.add_box(BoxSpec{
      .min = Point3d{0, 0, 0},
      .max = Point3d{2, 1, 3},
      .name = "demo_box",
  });

  Camera cam;
  cam.target = Point3d{1.0, 0.5, 1.5};
  create_camera(cam);

  create_renderable("demo_box", tessellate_body(*body), extract_edges(*body),
                    make_wood_material(wood_albedo_path),
                    Point3d{0, 0, 0});

  BREP_INFO(
      "demo scene: Document={} Part={} Body={} (guid={})",
      document_->guid.to_string(), part.guid.to_string(), body->name,
      body->guid.to_string());
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
