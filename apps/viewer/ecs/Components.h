#pragma once

#include "api/Core.h"
#include "api/Mesh.h"
#include "Camera.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>

namespace brep::viewer::ecs
{

struct Name
{
  std::string value;
};

struct Transform
{
  Point3d position{0, 0, 0};
  // Reserved for future rotation/scale; identity for now.
};

struct MeshComponent
{
  TriangleMesh triangles;
  EdgeMesh edges;
  bool dirty{true};
};

struct MaterialComponent
{
  Material material;
  bool dirty{true};
};

struct CameraComponent
{
  Camera camera;
};

/// Tag: the active view camera used by the render system.
struct MainCameraTag
{
};

/// Tag: entity should be drawn by the Vulkan render system.
struct RenderableTag
{
};

/// Links a renderable entity to a document Body Guid (for undo / selection).
struct BodyRef
{
  brep::Guid guid{};
};

/// Links a renderable to a parametric feature (optional).
struct FeatureRef
{
  brep::Guid feature_guid{};
};

/// Tag: currently selected renderable (supports multi-select).
struct SelectedTag
{
};

/// Active selection (stored in registry context).
struct SelectionState
{
  /// Last entity clicked / toggled (property panel focus).
  entt::entity primary{entt::null};
};

/// Shared CPU-side scene bake for multi-viewport upload.
struct RenderCache
{
  std::size_t renderable_count{0};
  entt::entity selection{entt::null};
  std::size_t selection_count{0};
  bool force_rebuild{false};
  std::uint64_t version{0};

  TriangleMesh scene_tri;
  EdgeMesh scene_edges;
  Material scene_material{};
  bool have_scene{false};

  TriangleMesh selected_tri;
  EdgeMesh selected_edges;
  EdgeMesh selected_outline;
  bool have_selection{false};
};

/// Transient input state (stored in registry context, not on an entity).
struct InputState
{
  /// Left: PendingSelect → click, or BoxSelect if dragged.
  /// Ctrl+Middle: Orbit. Middle/Right: Pan.
  enum class DragMode
  {
      None, PendingSelect, Orbit, Pan, BoxSelect 
  };

  DragMode drag_mode{DragMode::None};
  float last_x{0.0f};
  float last_y{0.0f};
  float press_x{0.0f};
  float press_y{0.0f};
  bool multi_select{false};
  bool camera_dirty{true};
};

}  // namespace brep::viewer::ecs
