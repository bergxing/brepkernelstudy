#pragma once

#include "brep/material.hpp"
#include "brep/math.hpp"
#include "brep/mesh.hpp"
#include "camera.hpp"

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

/// Transient input state (stored in registry context, not on an entity).
struct InputState {
  enum class DragMode { None, Orbit, Pan };

  DragMode drag_mode{DragMode::None};
  float last_x{0.0f};
  float last_y{0.0f};
  bool camera_dirty{true};
};

}  // namespace brep::viewer::ecs
