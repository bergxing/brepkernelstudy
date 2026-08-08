#pragma once

#include <entt/entt.hpp>

#include <cstdint>
#include <string>

namespace brep::viewer {
class VulkanRenderer;
class Camera;
}  // namespace brep::viewer

namespace brep::viewer::ecs {

/// Apply pointer/keyboard interaction to the given view camera.
/// Returns the active drag mode after press (for cursor feedback).
[[nodiscard]] int input_on_press(entt::registry& registry, float x, float y,
                                 int button);
void input_on_move(entt::registry& registry, Camera& camera, float x, float y,
                   int buttons);
/// Returns true when the gesture was a click (select), not a drag.
[[nodiscard]] bool input_on_release(entt::registry& registry);
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

void set_selection(entt::registry& registry, entt::entity entity);
void clear_selection(entt::registry& registry);
[[nodiscard]] entt::entity selected_entity(const entt::registry& registry);

/// Human-readable label for status bar (name + guid when available).
[[nodiscard]] std::string selection_label(const entt::registry& registry,
                                          entt::entity entity);

}  // namespace brep::viewer::ecs
