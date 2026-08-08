#pragma once

#include "ecs/world.hpp"
#include "vulkan_renderer.hpp"

#include <QVulkanWindow>

#include <functional>

namespace brep::viewer {

class VulkanWindow final : public QVulkanWindow {
  Q_OBJECT
 public:
  explicit VulkanWindow(QWindow* parent = nullptr);

  void set_world(ecs::World* world) noexcept { world_ = world; }
  [[nodiscard]] ecs::World* world() noexcept { return world_; }

  /// When false, left-click will not change selection (e.g. interactive tool).
  void set_selection_enabled(bool enabled) noexcept {
    selection_enabled_ = enabled;
  }

  using SelectionCallback = std::function<void(entt::entity)>;
  void set_selection_callback(SelectionCallback cb) {
    selection_callback_ = std::move(cb);
  }

  /// Called for mouse moves while an interactive tool owns the viewport
  /// (`selection_enabled_ == false`). Coordinates are in QWindow pixels.
  using ToolMotionCallback = std::function<void(float x, float y)>;
  void set_tool_motion_callback(ToolMotionCallback cb) {
    tool_motion_callback_ = std::move(cb);
  }

  /// Left-button press while a tool owns the viewport. Return true if consumed.
  using ToolPressCallback = std::function<bool(float x, float y, int button)>;
  void set_tool_press_callback(ToolPressCallback cb) {
    tool_press_callback_ = std::move(cb);
  }

  [[nodiscard]] Camera& camera() noexcept { return camera_; }
  [[nodiscard]] const Camera& camera() const noexcept { return camera_; }

  QVulkanWindowRenderer* createRenderer() override;
  bool eventFilter(QObject* watched, QEvent* event) override;

  /// Apply a wheel zoom step (positive = zoom in). Used by overlays / MainWindow.
  void handle_wheel(int angle_delta_y) { pointer_wheel(angle_delta_y); }

  void set_preview_edges(EdgeMesh edges);
  void clear_preview();

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void pointer_press(QPointF pos, Qt::MouseButton button,
                     Qt::KeyboardModifiers modifiers);
  void pointer_move(QPointF pos, Qt::MouseButtons buttons);
  void pointer_release(QPointF pos);
  void pointer_wheel(int angle_delta_y);
  void pointer_double_click(Qt::MouseButton button);
  void apply_key(int key);
  void sync_renderer();
  void maybe_select_at(float x, float y, bool multi);
  void restore_idle_cursor();
  void fit_view_to_scene();
  [[nodiscard]] bool forward_tool_press(QPointF pos, Qt::MouseButton button);

  ecs::World* world_{nullptr};
  Camera camera_{};
  VulkanRenderer* renderer_{nullptr};
  bool selection_enabled_{true};
  SelectionCallback selection_callback_;
  ToolMotionCallback tool_motion_callback_;
  ToolPressCallback tool_press_callback_;
};

}  // namespace brep::viewer
