#pragma once

#include "brep/math.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace brep::viewer {
class VulkanRenderer;
class Camera;
}  // namespace brep::viewer

namespace brep::viewer::ecs {

/// Apply pointer/keyboard interaction to the given view camera.
/// `modifiers` uses Qt::KeyboardModifier bits (e.g. Qt::ControlModifier).
/// Returns the active drag mode after press (for cursor feedback).
[[nodiscard]] int input_on_press(entt::registry& registry, float x, float y,
                                 int button, int modifiers);
void input_on_move(entt::registry& registry, Camera& camera, float x, float y,
                   int buttons);
/// Returns true when the gesture was a click (select), not a drag.
[[nodiscard]] bool input_on_release(entt::registry& registry);
/// True when the pending click should toggle multi-select (Ctrl held on press).
[[nodiscard]] bool pending_click_is_multi(const entt::registry& registry);
void input_on_wheel(entt::registry& registry, Camera& camera,
                    int angle_delta_y);
void input_on_key(entt::registry& registry, Camera& camera, int key);

/// Bake dirty mesh/material into shared cache, then upload to this renderer
/// when its `synced_version` lags behind the cache version.
void render_sync(entt::registry& registry, VulkanRenderer& renderer,
                 std::uint64_t& synced_version);

/// True when the camera was changed by input and a redraw is needed.
[[nodiscard]] bool consume_camera_dirty(entt::registry& registry);

/// Closest renderable under the cursor, or entt::null.
[[nodiscard]] entt::entity pick_renderable(entt::registry& registry,
                                           const Camera& cam, int viewport_w,
                                           int viewport_h, float sx, float sy);

/// Box pick in screen space. Left→right (x0<=x1): window (fully inside).
/// Right→left: crossing (screen AABB intersects rect).
[[nodiscard]] std::vector<entt::entity> pick_renderables_in_rect(
    entt::registry& registry, const Camera& cam, int viewport_w,
    int viewport_h, float x0, float y0, float x1, float y1);

/// World AABB of all renderables. Returns false when the scene is empty.
[[nodiscard]] bool scene_aabb(const entt::registry& registry, Point3d& out_min,
                              Point3d& out_max);

/// Zoom-to-fit all renderables into the camera (keeps view direction).
/// Empty scene → default home framing. `aspect` = width/height.
void fit_camera_to_scene(const entt::registry& registry, Camera& camera,
                         float aspect);

/// Replace selection with a single entity (or clear when null).
void set_selection(entt::registry& registry, entt::entity entity);
/// Ctrl+click: toggle entity in/out of the selection set.
void toggle_selection(entt::registry& registry, entt::entity entity);
/// Replace or union-select a set of entities (box select).
void select_entities(entt::registry& registry,
                     const std::vector<entt::entity>& entities, bool additive);
void clear_selection(entt::registry& registry);
[[nodiscard]] entt::entity selected_entity(const entt::registry& registry);
[[nodiscard]] std::size_t selected_count(const entt::registry& registry);

/// Human-readable label for status bar (name + guid when available).
[[nodiscard]] std::string selection_label(const entt::registry& registry,
                                          entt::entity entity);

}  // namespace brep::viewer::ecs
