#include "ecs/systems.hpp"

#include "ecs/components.hpp"
#include "vulkan_renderer.hpp"

#include <qnamespace.h>

#include <cmath>

namespace brep::viewer::ecs {
namespace {

CameraComponent* main_camera(entt::registry& registry) {
  auto view = registry.view<CameraComponent, MainCameraTag>();
  for (auto entity : view) {
    return &view.get<CameraComponent>(entity);
  }
  return nullptr;
}

InputState& input(entt::registry& registry) {
  return registry.ctx().get<InputState>();
}

}  // namespace

int input_on_press(entt::registry& registry, float x, float y, int button) {
  auto& state = input(registry);
  state.last_x = x;
  state.last_y = y;
  if (button == Qt::LeftButton) {
    state.drag_mode = InputState::DragMode::Orbit;
  } else if (button == Qt::RightButton || button == Qt::MiddleButton) {
    state.drag_mode = InputState::DragMode::Pan;
  } else {
    state.drag_mode = InputState::DragMode::None;
  }
  return static_cast<int>(state.drag_mode);
}

void input_on_move(entt::registry& registry, float x, float y, int buttons) {
  auto& state = input(registry);
  if (state.drag_mode == InputState::DragMode::None) return;

  if (buttons & Qt::LeftButton) {
    state.drag_mode = InputState::DragMode::Orbit;
  } else if (buttons & (Qt::RightButton | Qt::MiddleButton)) {
    state.drag_mode = InputState::DragMode::Pan;
  } else {
    return;
  }

  const float dx = x - state.last_x;
  const float dy = y - state.last_y;
  if (std::fabs(dx) < 1e-6f && std::fabs(dy) < 1e-6f) return;

  if (auto* cam = main_camera(registry)) {
    if (state.drag_mode == InputState::DragMode::Orbit) {
      cam->camera.orbit(dx, dy);
    } else {
      cam->camera.pan(dx, dy);
    }
    state.camera_dirty = true;
  }
  state.last_x = x;
  state.last_y = y;
}

void input_on_release(entt::registry& registry) {
  input(registry).drag_mode = InputState::DragMode::None;
}

void input_on_wheel(entt::registry& registry, int angle_delta_y) {
  if (angle_delta_y == 0) return;
  if (auto* cam = main_camera(registry)) {
    cam->camera.zoom(angle_delta_y > 0 ? 1.0f : -1.0f);
    input(registry).camera_dirty = true;
  }
}

void input_on_key(entt::registry& registry, int key) {
  auto* cam = main_camera(registry);
  if (!cam) return;

  constexpr float step = 8.0f;
  bool changed = true;
  switch (key) {
    case Qt::Key_Left:
      cam->camera.orbit(-step, 0.0f);
      break;
    case Qt::Key_Right:
      cam->camera.orbit(step, 0.0f);
      break;
    case Qt::Key_Up:
      cam->camera.orbit(0.0f, -step);
      break;
    case Qt::Key_Down:
      cam->camera.orbit(0.0f, step);
      break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      cam->camera.zoom(1.0f);
      break;
    case Qt::Key_Minus:
      cam->camera.zoom(-1.0f);
      break;
    default:
      changed = false;
      break;
  }
  if (changed) input(registry).camera_dirty = true;
}

void render_sync(entt::registry& registry, VulkanRenderer& renderer) {
  // Camera → renderer (via VulkanWindow camera getter; renderer reads it each frame).
  // Mesh / material push when dirty.
  auto view = registry.view<MeshComponent, MaterialComponent, RenderableTag>();
  for (auto entity : view) {
    auto& mesh = view.get<MeshComponent>(entity);
    auto& mat = view.get<MaterialComponent>(entity);
    if (mesh.dirty) {
      renderer.set_meshes(mesh.triangles, mesh.edges);
      mesh.dirty = false;
    }
    if (mat.dirty) {
      renderer.set_material(mat.material);
      mat.dirty = false;
    }
  }
}

bool consume_camera_dirty(entt::registry& registry) {
  auto& state = input(registry);
  const bool dirty = state.camera_dirty;
  state.camera_dirty = false;
  return dirty;
}

}  // namespace brep::viewer::ecs
