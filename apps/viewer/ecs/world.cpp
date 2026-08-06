#include "ecs/world.hpp"

#include "brep/brep.hpp"

namespace brep::viewer::ecs {

World::World() {
  registry_.ctx().emplace<InputState>();
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

void World::create_demo_box_scene(const std::string& wood_albedo_path) {
  using namespace brep;

  Camera cam;
  cam.target = Point3d{1.0, 0.5, 1.5};
  create_camera(cam);

  static Model model;
  Body* body = make_box(model, BoxSpec{
      .min = Point3d{0, 0, 0},
      .max = Point3d{2, 1, 3},
      .name = "demo_box",
  });

  create_renderable("demo_box", tessellate_body(*body), extract_edges(*body),
                    make_wood_material(wood_albedo_path),
                    Point3d{0, 0, 0});
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
