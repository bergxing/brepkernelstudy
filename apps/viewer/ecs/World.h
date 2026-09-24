#pragma once

#include "ecs/Components.h"

#include "api/Modeling.h"
#include "api/Persistence.h"

#include <entt/entt.hpp>

#include <memory>
#include <string>

namespace brep::viewer::ecs
{

/// Viewer world: EnTT registry + Document-backed scene bootstrap.
class World
{
 public:
  World();

  [[nodiscard]] entt::registry& registry() noexcept
  {
      return m_registry; 
  }
  [[nodiscard]] const entt::registry& registry() const noexcept
  {
    return m_registry;
  }

  [[nodiscard]] brep::Document* Document() noexcept
  {
      return m_document.get();
  }
  [[nodiscard]] const brep::Document* Document() const noexcept
  {
    return m_document.get();
  }

  /// Active Part model store (geometry/topology pools), if any.
  [[nodiscard]] brep::Model* model() noexcept;
  [[nodiscard]] const brep::Model* model() const noexcept;

  /// Destroy all ECS entities; keep InputState context.
  void ClearScene();

  /// Create orbit camera entity (tagged MainCamera).
  entt::entity CreateCamera(Camera camera);

  /// Create a renderable mesh entity with material.
  entt::entity CreateRenderable(std::string name, TriangleMesh triangles,
                                 EdgeMesh edges, Material material,
                                 Point3d position = {});

  /// Create renderable and attach BodyRef for Guid-based undo.
  entt::entity CreateBodyRenderable(std::string name, brep::Guid bodyGuid,
                                      TriangleMesh triangles, EdgeMesh edges,
                                      Material material,
                                      Point3d position = {},
                                      brep::Guid featureGuid = {});

  /// Destroy ECS renderable linked to a Body Guid (does not delete B-Rep).
  bool DestroyBodyRenderable(const brep::Guid& bodyGuid);

  [[nodiscard]] entt::entity FindBodyRenderable(
      const brep::Guid& bodyGuid) const;

  /// Refresh triangle/edge mesh for an existing body renderable.
  bool UpdateBodyRenderable(const brep::Guid& bodyGuid, TriangleMesh triangles,
                              EdgeMesh edges);

  /// Reconcile ECS renderables with Part bodies after regenerate / history.
  /// If cache is provided and contains a body Guid, tessellation is skipped.
  void SyncPartBodies(brep::Part& part, Material material,
                        const brep::io::BodyMeshCache* cache = nullptr);

  /// New Document → Part → camera only (blank document, no bodies).
  void CreateBlankScene(
      std::shared_ptr<brep::boolean::IBooleanEvaluator> evaluator = nullptr);

  /// New Document → Part → Body(demo box) + camera/renderables.
  void CreateDemoBoxScene(
      const std::string& wood_albedo_path,
      std::shared_ptr<brep::boolean::IBooleanEvaluator> evaluator = nullptr);

  /// Replace the active Document (e.g. after .xl load) and rebuild view.
  void AdoptDocument(std::unique_ptr<brep::Document> document,
                      Material material,
                      const brep::io::BodyMeshCache* cache = nullptr);

  [[nodiscard]] Camera* MainCamera() noexcept;
  [[nodiscard]] const Camera* MainCamera() const noexcept;

 private:
  entt::registry m_registry;
  std::unique_ptr<brep::Document> m_document;
};

}  // namespace brep::viewer::ecs
