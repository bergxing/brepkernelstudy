#include "ecs/Systems.h"

#include "commands/Picking.h"
#include "ecs/Components.h"
#include "VulkanRenderer.h"

#include <Qnamespace.h>

#include "Camera.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace brep::viewer::ecs
{
namespace
{

InputState& input(entt::registry& registry)
{
  return registry.ctx().get<InputState>();
}

SelectionState& selection(entt::registry& registry)
{
  if (!registry.ctx().contains<SelectionState>())
{
    registry.ctx().emplace<SelectionState>();
  }
  return registry.ctx().get<SelectionState>();
}

RenderCache& render_cache(entt::registry& registry)
{
  if (!registry.ctx().contains<RenderCache>())
{
    registry.ctx().emplace<RenderCache>();
  }
  return registry.ctx().get<RenderCache>();
}

constexpr float kSelectSlopPx = 5.0f;

void append_transformed_mesh(TriangleMesh& dst, const TriangleMesh& src,
                             const Point3d& offset)
{
  const auto base = static_cast<std::uint32_t>(dst.Vertices.size());
  dst.Vertices.reserve(dst.Vertices.size() + src.Vertices.size());
  for (const auto& v : src.Vertices)
  {
    MeshVertex out = v;
    out.Position = Point3d{v.Position.x() + offset.x(),
                           v.Position.y() + offset.y(),
                           v.Position.z() + offset.z()};
    dst.Vertices.push_back(out);
  }
  dst.Indices.reserve(dst.Indices.size() + src.Indices.size());
  for (const auto idx : src.Indices)
  {
    dst.Indices.push_back(base + idx);
  }
}

void append_transformed_edges(EdgeMesh& dst, const EdgeMesh& src,
                              const Point3d& offset)
{
  dst.Positions.reserve(dst.Positions.size() + src.Positions.size());
  for (const auto& p : src.Positions)
  {
    dst.Positions.push_back(Point3d{p.x() + offset.x(), p.y() + offset.y(),
                                    p.z() + offset.z()});
  }
}

Material make_selection_material()
{
  Material m;
  m.Name = "selection";
  m.AlbedoPath.clear();
  m.UvScale = 1.0f;
  // Bright orange — clearly distinct from default blue fallback / wood.
  m.AlbedoColor[0] = 1.0f;
  m.AlbedoColor[1] = 0.45f;
  m.AlbedoColor[2] = 0.08f;
  return m;
}

}  // namespace

int input_on_press(entt::registry& registry, float x, float y, int button,
                   int modifiers)
{
  auto& state = input(registry);
  state.last_x = x;
  state.last_y = y;
  state.press_x = x;
  state.press_y = y;
  state.multi_select = (modifiers & Qt::ControlModifier) != 0;

  if (button == Qt::LeftButton)
  {
    // Click without drag → select. Left-drag no longer orbits.
    state.drag_mode = InputState::DragMode::PendingSelect;
  } else if (button == Qt::MiddleButton && state.multi_select)
  {
    state.drag_mode = InputState::DragMode::Orbit;
  } else if (button == Qt::MiddleButton)
  {
    // Pan is middle-button only; right-click is reserved for context menu.
    state.drag_mode = InputState::DragMode::Pan;
  }
  else
  {
    state.drag_mode = InputState::DragMode::None;
  }
  return static_cast<int>(state.drag_mode);
}

void input_on_move(entt::registry& registry, Camera& camera, float x, float y,
                   int buttons)
{
  auto& state = input(registry);
  if (state.drag_mode == InputState::DragMode::None) return;

  if (state.drag_mode == InputState::DragMode::PendingSelect)
  {
    if (!(buttons & Qt::LeftButton)) return;
    const float dx = x - state.press_x;
    const float dy = y - state.press_y;
    if (dx * dx + dy * dy < kSelectSlopPx * kSelectSlopPx)
    {
      state.last_x = x;
      state.last_y = y;
      return;
    }
    // Dragged with LMB: start rubber-band box select.
    state.drag_mode = InputState::DragMode::BoxSelect;
    state.last_x = x;
    state.last_y = y;
    return;
  } else if (state.drag_mode == InputState::DragMode::BoxSelect)
  {
    if (!(buttons & Qt::LeftButton)) return;
    state.last_x = x;
    state.last_y = y;
    return;
  } else if (state.drag_mode == InputState::DragMode::Orbit)
  {
    if (!(buttons & Qt::MiddleButton)) return;
  } else if (state.drag_mode == InputState::DragMode::Pan)
  {
    if (!(buttons & Qt::MiddleButton)) return;
  }

  const float dx = x - state.last_x;
  const float dy = y - state.last_y;
  if (std::fabs(dx) < 1e-6f && std::fabs(dy) < 1e-6f) return;

  if (state.drag_mode == InputState::DragMode::Orbit)
  {
    camera.orbit(dx, dy);
  } else if (state.drag_mode == InputState::DragMode::Pan)
  {
    camera.pan(dx, dy);
  }
  state.camera_dirty = true;
  state.last_x = x;
  state.last_y = y;
}

bool input_on_release(entt::registry& registry)
{
  auto& state = input(registry);
  const bool click = state.drag_mode == InputState::DragMode::PendingSelect;
  state.drag_mode = InputState::DragMode::None;
  return click;
}

bool pending_click_is_multi(const entt::registry& registry)
{
  if (!registry.ctx().contains<InputState>()) return false;
  return registry.ctx().get<InputState>().multi_select;
}

void input_on_wheel(entt::registry& registry, Camera& camera,
                    int angle_delta_y)
{
  if (angle_delta_y == 0) return;
  camera.zoom(angle_delta_y > 0 ? 1.0f : -1.0f);
  input(registry).camera_dirty = true;
}

void input_on_key(entt::registry& registry, Camera& camera, int key)
{
  constexpr float step = 8.0f;
  bool changed = true;
  switch (key)
  {
    case Qt::Key_Left:
      camera.orbit(-step, 0.0f);
      break;
    case Qt::Key_Right:
      camera.orbit(step, 0.0f);
      break;
    case Qt::Key_Up:
      camera.orbit(0.0f, -step);
      break;
    case Qt::Key_Down:
      camera.orbit(0.0f, step);
      break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      camera.zoom(1.0f);
      break;
    case Qt::Key_Minus:
      camera.zoom(-1.0f);
      break;
    default:
      changed = false;
      break;
  }
  if (changed) input(registry).camera_dirty = true;
}

void render_sync(entt::registry& registry, VulkanRenderer& renderer,
                 std::uint64_t& synced_version)
{
  auto view =
      registry.view<MeshComponent, MaterialComponent, Transform, RenderableTag>();

  std::size_t count = 0;
  bool any_component_dirty = false;
  for (auto entity : view)
  {
    ++count;
    if (view.get<MeshComponent>(entity).dirty ||
        view.get<MaterialComponent>(entity).dirty)
    {
      any_component_dirty = true;
    }
  }

  auto& cache = render_cache(registry);
  const entt::entity sel = selected_entity(registry);
  const std::size_t sel_count = selected_count(registry);
  const bool need_rebuild =
      any_component_dirty || count != cache.renderable_count ||
      sel != cache.selection || sel_count != cache.selection_count ||
      cache.force_rebuild;

  if (need_rebuild)
  {
    cache.scene_tri = {};
    cache.scene_edges = {};
    cache.selected_tri = {};
    cache.selected_edges = {};
    cache.selected_outline = {};
    cache.scene_material = {};
    cache.have_scene = false;
    cache.have_selection = false;
    bool have_material = false;

    for (auto entity : view)
    {
      auto& mesh = view.get<MeshComponent>(entity);
      auto& mat = view.get<MaterialComponent>(entity);
      const auto& xform = view.get<Transform>(entity);
      const bool selected = registry.all_of<SelectedTag>(entity);

      if (selected)
      {
        append_transformed_mesh(cache.selected_tri, mesh.triangles,
                                xform.position);
        append_transformed_edges(cache.selected_edges, mesh.edges,
                                 xform.position);
      }
      else
      {
        append_transformed_mesh(cache.scene_tri, mesh.triangles, xform.position);
        append_transformed_edges(cache.scene_edges, mesh.edges, xform.position);
        if (!have_material)
        {
          cache.scene_material = mat.material;
          have_material = true;
        } else if (cache.scene_material.AlbedoPath.empty() &&
                   !mat.material.AlbedoPath.empty())
        {
          cache.scene_material = mat.material;
        }
      }

      mesh.dirty = false;
      mat.dirty = false;
    }

    if (!have_material)
    {
      for (auto entity : view)
    {
        cache.scene_material = view.get<MaterialComponent>(entity).material;
        have_material = true;
        if (!cache.scene_material.AlbedoPath.empty()) break;
      }
    }

    cache.have_scene = !(cache.scene_tri.Indices.empty() &&
                         cache.scene_edges.Positions.empty());
    cache.have_selection = !(cache.selected_tri.Indices.empty() &&
                             cache.selected_edges.Positions.empty());
    if (cache.have_selection)
    {
      cache.selected_outline = cache.selected_edges;
    }

    cache.renderable_count = count;
    cache.selection = sel;
    cache.selection_count = sel_count;
    cache.force_rebuild = false;
    ++cache.version;
  }

  if (synced_version == cache.version) return;

  if (!cache.have_scene)
  {
    renderer.set_meshes({}, {});
  }
  else
  {
    renderer.set_meshes(cache.scene_tri, cache.scene_edges);
    renderer.set_material(cache.scene_material);
  }

  if (!cache.have_selection)
  {
    renderer.clear_selection_mesh();
    renderer.clear_highlight();
  }
  else
  {
    renderer.set_selection_mesh(cache.selected_tri, cache.selected_edges,
                                make_selection_material());
    renderer.set_highlight_edges(cache.selected_outline);
  }

  synced_version = cache.version;
}

bool consume_camera_dirty(entt::registry& registry)
{
  auto& state = input(registry);
  const bool dirty = state.camera_dirty;
  state.camera_dirty = false;
  return dirty;
}

bool scene_aabb(const entt::registry& registry, Point3d& out_min,
                Point3d& out_max)
{
  bool any = false;
  auto view = registry.view<const MeshComponent, const Transform,
                            const RenderableTag>();
  for (auto entity : view)
  {
    const auto& mesh = view.get<const MeshComponent>(entity);
    const auto& xform = view.get<const Transform>(entity);
    for (const auto& v : mesh.triangles.Vertices)
    {
      const Point3d p{v.Position.x() + xform.position.x(),
                      v.Position.y() + xform.position.y(),
                      v.Position.z() + xform.position.z()};
      if (!any)
      {
        out_min = out_max = p;
        any = true;
      }
      else
      {
        out_min = Point3d{std::min(out_min.x(), p.x()),
                          std::min(out_min.y(), p.y()),
                          std::min(out_min.z(), p.z())};
        out_max = Point3d{std::max(out_max.x(), p.x()),
                          std::max(out_max.y(), p.y()),
                          std::max(out_max.z(), p.z())};
      }
    }
  }
  return any;
}

void fit_camera_to_scene(const entt::registry& registry, Camera& camera,
                         float aspect)
{
  Point3d bmin;
  Point3d bmax;
  if (!scene_aabb(registry, bmin, bmax))
  {
    camera.target = Point3d{0.0, 0.0, 0.0};
    if (camera.ortho)
    {
      camera.ortho_half_h = 2.1f;
      camera.distance = 6.0f;
    }
    else
    {
      camera.distance = 6.0f;
    }
    return;
  }

  const Point3d center{(bmin.x() + bmax.x()) * 0.5,
                       (bmin.y() + bmax.y()) * 0.5,
                       (bmin.z() + bmax.z()) * 0.5};
  const Vector3d ext{bmax.x() - bmin.x(), bmax.y() - bmin.y(),
                     bmax.z() - bmin.z()};
  const float radius =
      std::max(0.05f, static_cast<float>(ext.norm() * 0.5));
  camera.fit_sphere(center, radius, aspect);
}

entt::entity pick_renderable(entt::registry& registry, const Camera& cam,
                             int viewport_w, int viewport_h, float sx,
                             float sy)
                             {
  Point3d origin;
  Vector3d dir;
  if (!commands::screen_to_ray(cam, viewport_w, viewport_h, sx, sy, origin,
                               dir))
  {
    return entt::null;
  }

  entt::entity best = entt::null;
  double best_t = std::numeric_limits<double>::infinity();

  auto view = registry.view<MeshComponent, Transform, RenderableTag>();
  for (auto entity : view)
  {
    const auto& mesh = view.get<MeshComponent>(entity);
    const auto& xform = view.get<Transform>(entity);
    double t = 0.0;
    if (!commands::intersect_mesh(origin, dir, mesh.triangles, xform.position,
                                  t))
    {
      continue;
    }
    if (t < best_t)
    {
      best_t = t;
      best = entity;
    }
  }
  return best;
}

namespace
{

bool entity_world_aabb(const MeshComponent& mesh, const Transform& xform,
                       Point3d& out_min, Point3d& out_max)
{
  if (mesh.triangles.Vertices.empty()) return false;
  bool any = false;
  for (const auto& v : mesh.triangles.Vertices)
  {
    const Point3d p{v.Position.x() + xform.position.x(),
                    v.Position.y() + xform.position.y(),
                    v.Position.z() + xform.position.z()};
    if (!any)
    {
      out_min = out_max = p;
      any = true;
    }
    else
    {
      out_min = Point3d{std::min(out_min.x(), p.x()),
                        std::min(out_min.y(), p.y()),
                        std::min(out_min.z(), p.z())};
      out_max = Point3d{std::max(out_max.x(), p.x()),
                        std::max(out_max.y(), p.y()),
                        std::max(out_max.z(), p.z())};
    }
  }
  return any;
}

bool project_aabb_to_screen(const Camera& cam, int viewport_w, int viewport_h,
                            const Point3d& bmin, const Point3d& bmax,
                            float& out_min_x, float& out_min_y,
                            float& out_max_x, float& out_max_y)
                            {
  const Point3d corners[8] = {
      {bmin.x(), bmin.y(), bmin.z()}, {bmax.x(), bmin.y(), bmin.z()},
      {bmin.x(), bmax.y(), bmin.z()}, {bmax.x(), bmax.y(), bmin.z()},
      {bmin.x(), bmin.y(), bmax.z()}, {bmax.x(), bmin.y(), bmax.z()},
      {bmin.x(), bmax.y(), bmax.z()}, {bmax.x(), bmax.y(), bmax.z()},
  };
  bool any = false;
  for (const auto& c : corners)
  {
    float sx = 0.0f;
    float sy = 0.0f;
    if (!commands::world_to_screen(cam, viewport_w, viewport_h, c, sx, sy))
    {
      continue;
    }
    if (!any)
    {
      out_min_x = out_max_x = sx;
      out_min_y = out_max_y = sy;
      any = true;
    }
    else
    {
      out_min_x = std::min(out_min_x, sx);
      out_max_x = std::max(out_max_x, sx);
      out_min_y = std::min(out_min_y, sy);
      out_max_y = std::max(out_max_y, sy);
    }
  }
  return any;
}

}  // namespace

std::vector<entt::entity> pick_renderables_in_rect(
    entt::registry& registry, const Camera& cam, int viewport_w,
    int viewport_h, float x0, float y0, float x1, float y1)
    {
  std::vector<entt::entity> hits;
  const float sel_min_x = std::min(x0, x1);
  const float sel_max_x = std::max(x0, x1);
  const float sel_min_y = std::min(y0, y1);
  const float sel_max_y = std::max(y0, y1);
  if (sel_max_x - sel_min_x < 1.0f || sel_max_y - sel_min_y < 1.0f)
  {
    return hits;
  }

  // Left→right: window (fully inside). Right→left: crossing (intersects).
  const bool window_mode = x1 >= x0;

  auto view = registry.view<MeshComponent, Transform, RenderableTag>();
  for (auto entity : view)
  {
    Point3d bmin;
    Point3d bmax;
    if (!entity_world_aabb(view.get<MeshComponent>(entity),
                           view.get<Transform>(entity), bmin, bmax))
    {
      continue;
    }
    float omin_x = 0, omin_y = 0, omax_x = 0, omax_y = 0;
    if (!project_aabb_to_screen(cam, viewport_w, viewport_h, bmin, bmax, omin_x,
                                omin_y, omax_x, omax_y))
    {
      continue;
    }

    const bool intersects =
        !(omax_x < sel_min_x || omin_x > sel_max_x || omax_y < sel_min_y ||
          omin_y > sel_max_y);
    if (!intersects) continue;

    if (window_mode)
    {
      const bool contained = omin_x >= sel_min_x && omax_x <= sel_max_x &&
                             omin_y >= sel_min_y && omax_y <= sel_max_y;
      if (!contained) continue;
    }
    hits.push_back(entity);
  }
  return hits;
}

namespace
{

void clear_selected_tags(entt::registry& registry)
{
  auto view = registry.view<SelectedTag>();
  std::vector<entt::entity> tagged;
  for (auto entity : view) tagged.push_back(entity);
  for (auto entity : tagged)
  {
    registry.remove<SelectedTag>(entity);
  }
}

}  // namespace

void set_selection(entt::registry& registry, entt::entity entity)
{
  auto& sel = selection(registry);
  clear_selected_tags(registry);

  if (entity == entt::null || !registry.valid(entity))
  {
    sel.primary = entt::null;
  }
  else
  {
    sel.primary = entity;
    registry.emplace<SelectedTag>(entity);
  }

  // Must force rebuild: clearing selection sets both sel and cache to null,
  // which would otherwise look like "no change".
  render_cache(registry).force_rebuild = true;
}

void toggle_selection(entt::registry& registry, entt::entity entity)
{
  if (entity == entt::null || !registry.valid(entity)) return;

  auto& sel = selection(registry);
  if (registry.all_of<SelectedTag>(entity))
  {
    registry.remove<SelectedTag>(entity);
    if (sel.primary == entity)
    {
      sel.primary = entt::null;
      auto view = registry.view<SelectedTag>();
      for (auto e : view)
      {
        sel.primary = e;
        break;
      }
    }
  }
  else
      {
    registry.emplace<SelectedTag>(entity);
    sel.primary = entity;
  }
  render_cache(registry).force_rebuild = true;
}

void select_entities(entt::registry& registry,
                     const std::vector<entt::entity>& entities, bool additive)
{
  auto& sel = selection(registry);
  if (!additive)
  {
    clear_selected_tags(registry);
    sel.primary = entt::null;
  }

  for (auto entity : entities)
  {
    if (entity == entt::null || !registry.valid(entity)) continue;
    if (!registry.all_of<SelectedTag>(entity))
    {
      registry.emplace<SelectedTag>(entity);
    }
    sel.primary = entity;
  }

  if (!additive && entities.empty())
  {
    sel.primary = entt::null;
  }
  render_cache(registry).force_rebuild = true;
}

void clear_selection(entt::registry& registry)
{
  set_selection(registry, entt::null);
}

entt::entity selected_entity(const entt::registry& registry)
{
  if (!registry.ctx().contains<SelectionState>()) return entt::null;
  return registry.ctx().get<SelectionState>().primary;
}

std::size_t selected_count(const entt::registry& registry)
{
  return registry.view<SelectedTag>().size();
}

std::string selection_label(const entt::registry& registry,
                            entt::entity entity)
{
  if (entity == entt::null || !registry.valid(entity)) return {};
  std::string label;
  if (const auto* name = registry.try_get<Name>(entity))
  {
    label = name->value;
  }
  if (const auto* body = registry.try_get<BodyRef>(entity))
  {
    if (!label.empty()) label += "  ";
    label += body->guid.ToString();
  }
  if (label.empty()) label = "entity";
  return label;
}

}  // namespace brep::viewer::ecs
