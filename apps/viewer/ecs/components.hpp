#pragma once

#include "brep/guid.hpp"
#include "brep/material.hpp"
#include "brep/math.hpp"
#include "brep/mesh.hpp"
#include "camera.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>

namespace brep::viewer::ecs {

struct Name {
  std::string value;
};

struct Transform {
  Point3d position{0, 0, 0};
  // Reserved for future rotation/scale; identity for now.
};

struct MeshComponent {
  TriangleMesh triangles;
  EdgeMesh edges;
  bool dirty{true};
};

struct MaterialComponent {
  Material material;
  bool dirty{true};
};

struct CameraComponent {
  Camera camera;
};

/// Tag: the active view camera used by the render system.
struct MainCameraTag {};

/// Tag: entity should be drawn by the Vulkan render system.
struct RenderableTag {};

/// Links a renderable entity to a document Body Guid (for undo / selection).
struct BodyRef {
  brep::Guid guid{};
};

/// Links a renderable to a parametric feature (optional).
struct FeatureRef {
  brep::Guid feature_guid{};
};

/// Tag: currently selected renderable (single-selection for now).
struct SelectedTag {};

/// Active selection (stored in registry context).
struct SelectionState {
  entt::entity primary{entt::null};
};

/// Shared CPU-side scene bake for multi-viewport upload.
struct RenderCache {
  std::size_t renderable_count{0};
  entt::entity selection{entt::null};
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
struct InputState {
  /// Left-press starts as PendingSelect; crosses slop → Orbit. Click = select.
  enum class DragMode { None, PendingSelect, Orbit, Pan };

  DragMode drag_mode{DragMode::None};
  float last_x{0.0f};
  float last_y{0.0f};
  float press_x{0.0f};
  float press_y{0.0f};
  bool camera_dirty{true};
};

}  // namespace brep::viewer::ecs
