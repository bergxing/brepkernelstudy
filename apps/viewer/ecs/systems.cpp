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

namespace {

void append_transformed_mesh(TriangleMesh& dst, const TriangleMesh& src,
                             const Point3d& offset) {
  const auto base = static_cast<std::uint32_t>(dst.vertices.size());
  dst.vertices.reserve(dst.vertices.size() + src.vertices.size());
  for (const auto& v : src.vertices) {
    MeshVertex out = v;
    out.position = Point3d{v.position.x() + offset.x(),
                           v.position.y() + offset.y(),
                           v.position.z() + offset.z()};
    dst.vertices.push_back(out);
  }
  dst.indices.reserve(dst.indices.size() + src.indices.size());
  for (const auto idx : src.indices) {
    dst.indices.push_back(base + idx);
  }
}

void append_transformed_edges(EdgeMesh& dst, const EdgeMesh& src,
                              const Point3d& offset) {
  dst.positions.reserve(dst.positions.size() + src.positions.size());
  for (const auto& p : src.positions) {
    dst.positions.push_back(Point3d{p.x() + offset.x(), p.y() + offset.y(),
                                    p.z() + offset.z()});
  }
}

RenderCache& render_cache(entt::registry& registry) {
  if (!registry.ctx().contains<RenderCache>()) {
    registry.ctx().emplace<RenderCache>();
  }
  return registry.ctx().get<RenderCache>();
}

}  // namespace

void render_sync(entt::registry& registry, VulkanRenderer& renderer) {
  auto view =
      registry.view<MeshComponent, MaterialComponent, Transform, RenderableTag>();

  std::size_t count = 0;
  bool any_component_dirty = false;
  for (auto entity : view) {
    ++count;
    if (view.get<MeshComponent>(entity).dirty ||
        view.get<MaterialComponent>(entity).dirty) {
      any_component_dirty = true;
    }
  }

  auto& cache = render_cache(registry);
  const entt::entity sel = selected_entity(registry);
  const bool need_rebuild =
      any_component_dirty || count != cache.renderable_count ||
      sel != cache.selection;

  if (!need_rebuild) return;

  TriangleMesh combined_tri;
  EdgeMesh combined_edges;
  EdgeMesh highlight;
  Material scene_material{};
  bool have_material = false;

  for (auto entity : view) {
    auto& mesh = view.get<MeshComponent>(entity);
    auto& mat = view.get<MaterialComponent>(entity);
    const auto& xform = view.get<Transform>(entity);

    append_transformed_mesh(combined_tri, mesh.triangles, xform.position);
    append_transformed_edges(combined_edges, mesh.edges, xform.position);

    if (!have_material) {
      scene_material = mat.material;
      have_material = true;
    } else if (scene_material.albedo_path.empty() &&
               !mat.material.albedo_path.empty()) {
      scene_material = mat.material;
    }

    if (registry.all_of<SelectedTag>(entity)) {
      append_transformed_edges(highlight, mesh.edges, xform.position);
    }

    mesh.dirty = false;
    mat.dirty = false;
  }

  if (count == 0) {
    renderer.set_meshes({}, {});
    renderer.clear_highlight();
  } else {
    renderer.set_meshes(std::move(combined_tri), std::move(combined_edges));
    if (have_material) renderer.set_material(scene_material);
    if (highlight.positions.empty()) {
      renderer.clear_highlight();
    } else {
      renderer.set_highlight_edges(std::move(highlight));
    }
  }

  cache.renderable_count = count;
  cache.selection = sel;
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
  }

  sel.primary = entity;
  if (entity == entt::null || !registry.valid(entity)) {
    sel.primary = entt::null;
  } else {
    registry.emplace_or_replace<SelectedTag>(entity);
  }

  // Force scene rebuild so the orange highlight outline updates.
  render_cache(registry).selection = entt::null;
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
