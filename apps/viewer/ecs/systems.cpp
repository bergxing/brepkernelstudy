#include "ecs/systems.hpp"

#include "commands/picking.hpp"
#include "ecs/components.hpp"
#include "vulkan_renderer.hpp"

#include <qnamespace.h>

#include <cmath>
#include <limits>

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

SelectionState& selection(entt::registry& registry) {
  if (!registry.ctx().contains<SelectionState>()) {
    registry.ctx().emplace<SelectionState>();
  }
  return registry.ctx().get<SelectionState>();
}

constexpr float kSelectSlopPx = 5.0f;

}  // namespace

int input_on_press(entt::registry& registry, float x, float y, int button) {
  auto& state = input(registry);
  state.last_x = x;
  state.last_y = y;
  state.press_x = x;
  state.press_y = y;
  if (button == Qt::LeftButton) {
    // Click without drag → select; drag past slop → orbit.
    state.drag_mode = InputState::DragMode::PendingSelect;
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

  if (state.drag_mode == InputState::DragMode::PendingSelect) {
    if (!(buttons & Qt::LeftButton)) return;
    const float dx = x - state.press_x;
    const float dy = y - state.press_y;
    if (dx * dx + dy * dy < kSelectSlopPx * kSelectSlopPx) {
      state.last_x = x;
      state.last_y = y;
      return;
    }
    state.drag_mode = InputState::DragMode::Orbit;
  } else if (state.drag_mode == InputState::DragMode::Orbit) {
    if (!(buttons & Qt::LeftButton)) return;
  } else if (state.drag_mode == InputState::DragMode::Pan) {
    if (!(buttons & (Qt::RightButton | Qt::MiddleButton))) return;
  }

  const float dx = x - state.last_x;
  const float dy = y - state.last_y;
  if (std::fabs(dx) < 1e-6f && std::fabs(dy) < 1e-6f) return;

  if (auto* cam = main_camera(registry)) {
    if (state.drag_mode == InputState::DragMode::Orbit) {
      cam->camera.orbit(dx, dy);
    } else if (state.drag_mode == InputState::DragMode::Pan) {
      cam->camera.pan(dx, dy);
    }
    state.camera_dirty = true;
  }
  state.last_x = x;
  state.last_y = y;
}

bool input_on_release(entt::registry& registry) {
  auto& state = input(registry);
  const bool click = state.drag_mode == InputState::DragMode::PendingSelect;
  state.drag_mode = InputState::DragMode::None;
  return click;
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
  auto view =
      registry.view<MeshComponent, MaterialComponent, Transform, RenderableTag>();
  bool any = false;
  for (auto entity : view) {
    any = true;
    auto& mesh = view.get<MeshComponent>(entity);
    auto& mat = view.get<MaterialComponent>(entity);
    if (mesh.dirty) {
      renderer.set_meshes(mesh.triangles, mesh.edges);
      mesh.dirty = false;
    }
    if (mat.dirty) {
      Material upload = mat.material;
      if (registry.all_of<SelectedTag>(entity)) {
        // Solid highlight so selection is obvious even with textured albedo.
        upload.name = "selected";
        upload.albedo_path.clear();
        upload.albedo_color[0] = 1.0f;
        upload.albedo_color[1] = 0.62f;
        upload.albedo_color[2] = 0.12f;
      }
      renderer.set_material(upload);
      mat.dirty = false;
    }
  }
  // Blank documents have no renderables — clear leftover GPU meshes.
  if (!any) {
    renderer.set_meshes({}, {});
  }
}

bool consume_camera_dirty(entt::registry& registry) {
  auto& state = input(registry);
  const bool dirty = state.camera_dirty;
  state.camera_dirty = false;
  return dirty;
}

entt::entity pick_renderable(entt::registry& registry, const Camera& cam,
                             int viewport_w, int viewport_h, float sx,
                             float sy) {
  Point3d origin;
  Vector3d dir;
  if (!commands::screen_to_ray(cam, viewport_w, viewport_h, sx, sy, origin,
                               dir)) {
    return entt::null;
  }

  entt::entity best = entt::null;
  double best_t = std::numeric_limits<double>::infinity();

  auto view = registry.view<MeshComponent, Transform, RenderableTag>();
  for (auto entity : view) {
    const auto& mesh = view.get<MeshComponent>(entity);
    const auto& xform = view.get<Transform>(entity);
    double t = 0.0;
    if (!commands::intersect_mesh(origin, dir, mesh.triangles, xform.position,
                                  t)) {
      continue;
    }
    if (t < best_t) {
      best_t = t;
      best = entity;
    }
  }
  return best;
}

void set_selection(entt::registry& registry, entt::entity entity) {
  auto& sel = selection(registry);
  if (sel.primary == entity) return;

  if (sel.primary != entt::null && registry.valid(sel.primary)) {
    registry.remove<SelectedTag>(sel.primary);
    if (auto* mat = registry.try_get<MaterialComponent>(sel.primary)) {
      mat->dirty = true;
    }
  }

  sel.primary = entity;
  if (entity == entt::null || !registry.valid(entity)) {
    sel.primary = entt::null;
    return;
  }

  registry.emplace_or_replace<SelectedTag>(entity);
  if (auto* mat = registry.try_get<MaterialComponent>(entity)) {
    mat->dirty = true;
  }
  // With the single-mesh renderer, make the selected body the active GPU mesh.
  if (auto* mesh = registry.try_get<MeshComponent>(entity)) {
    mesh->dirty = true;
  }
}

void clear_selection(entt::registry& registry) {
  set_selection(registry, entt::null);
}

entt::entity selected_entity(const entt::registry& registry) {
  if (!registry.ctx().contains<SelectionState>()) return entt::null;
  return registry.ctx().get<SelectionState>().primary;
}

std::string selection_label(const entt::registry& registry,
                            entt::entity entity) {
  if (entity == entt::null || !registry.valid(entity)) return {};
  std::string label;
  if (const auto* name = registry.try_get<Name>(entity)) {
    label = name->value;
  }
  if (const auto* body = registry.try_get<BodyRef>(entity)) {
    if (!label.empty()) label += "  ";
    label += body->guid.to_string();
  }
  if (label.empty()) label = "entity";
  return label;
}

}  // namespace brep::viewer::ecs
