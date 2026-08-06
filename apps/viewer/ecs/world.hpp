#pragma once

#include "ecs/components.hpp"

#include <entt/entt.hpp>

#include <string>

namespace brep::viewer::ecs {

/// Viewer world: EnTT registry + scene bootstrap helpers.
class World {
 public:
  World();

  [[nodiscard]] entt::registry& registry() noexcept { return registry_; }
  [[nodiscard]] const entt::registry& registry() const noexcept {
    return registry_;
  }

  /// Create orbit camera entity (tagged MainCamera).
  entt::entity create_camera(Camera camera);

  /// Create a renderable mesh entity with material.
  entt::entity create_renderable(std::string name, TriangleMesh triangles,
                                 EdgeMesh edges, Material material,
                                 Point3d position = {});

  /// Convenience: demo wood box scene used by MainWindow.
  void create_demo_box_scene(const std::string& wood_albedo_path);

  [[nodiscard]] Camera* main_camera() noexcept;
  [[nodiscard]] const Camera* main_camera() const noexcept;

 private:
  entt::registry registry_;
};

}  // namespace brep::viewer::ecs
