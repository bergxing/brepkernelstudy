#pragma once

#include "ecs/components.hpp"

#include "brep/document.hpp"

#include <entt/entt.hpp>

#include <memory>
#include <string>

namespace brep::viewer::ecs {

/// Viewer world: EnTT registry + Document-backed scene bootstrap.
class World {
 public:
  World();

  [[nodiscard]] entt::registry& registry() noexcept { return registry_; }
  [[nodiscard]] const entt::registry& registry() const noexcept {
    return registry_;
  }

  [[nodiscard]] brep::Document* document() noexcept { return document_.get(); }
  [[nodiscard]] const brep::Document* document() const noexcept {
    return document_.get();
  }

  /// Active Part model store (geometry/topology pools), if any.
  [[nodiscard]] brep::Model* model() noexcept;
  [[nodiscard]] const brep::Model* model() const noexcept;

  /// Destroy all ECS entities; keep InputState context.
  void clear_scene();

  /// Create orbit camera entity (tagged MainCamera).
  entt::entity create_camera(Camera camera);

  /// Create a renderable mesh entity with material.
  entt::entity create_renderable(std::string name, TriangleMesh triangles,
                                 EdgeMesh edges, Material material,
                                 Point3d position = {});

  /// Create renderable and attach BodyRef for Guid-based undo.
  entt::entity create_body_renderable(std::string name, brep::Guid body_guid,
                                      TriangleMesh triangles, EdgeMesh edges,
                                      Material material,
                                      Point3d position = {},
                                      brep::Guid feature_guid = {});

  /// Destroy ECS renderable linked to a Body Guid (does not delete B-Rep).
  bool destroy_body_renderable(const brep::Guid& body_guid);

  [[nodiscard]] entt::entity find_body_renderable(
      const brep::Guid& body_guid) const;

  /// Refresh triangle/edge mesh for an existing body renderable.
  bool update_body_renderable(const brep::Guid& body_guid, TriangleMesh triangles,
                              EdgeMesh edges);

  /// Reconcile ECS renderables with Part bodies after regenerate / history.
  void sync_part_bodies(brep::Part& part, Material material);

  /// New Document → Part → camera only (blank document, no bodies).
  void create_blank_scene();

  /// New Document → Part → Body(demo box) + camera/renderables.
  void create_demo_box_scene(const std::string& wood_albedo_path);

  [[nodiscard]] Camera* main_camera() noexcept;
  [[nodiscard]] const Camera* main_camera() const noexcept;

 private:
  entt::registry registry_;
  std::unique_ptr<brep::Document> document_;
};

}  // namespace brep::viewer::ecs
