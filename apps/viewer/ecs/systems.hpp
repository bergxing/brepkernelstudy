#pragma once

#include <entt/entt.hpp>

namespace brep::viewer {
class VulkanRenderer;
}

namespace brep::viewer::ecs {

/// Apply pointer/keyboard interaction to the MainCamera entity.
/// Returns the active drag mode after press (for cursor feedback).
[[nodiscard]] int input_on_press(entt::registry& registry, float x, float y,
                                 int button);
void input_on_move(entt::registry& registry, float x, float y, int buttons);
void input_on_release(entt::registry& registry);
void input_on_wheel(entt::registry& registry, int angle_delta_y);
void input_on_key(entt::registry& registry, int key);

/// Push dirty mesh/material/camera components into the Vulkan backend.
void render_sync(entt::registry& registry, VulkanRenderer& renderer);

/// True when the camera was changed by input and a redraw is needed.
[[nodiscard]] bool consume_camera_dirty(entt::registry& registry);

}  // namespace brep::viewer::ecs
