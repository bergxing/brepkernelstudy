#pragma once

#include "ecs/World.h"
#include "VulkanRenderer.h"

#include <QVulkanWindow>

#include <functional>

class QWidget;

namespace brep::viewer
{

class SelectRectOverlay;

class VulkanWindow final : public QVulkanWindow
{
  Q_OBJECT
 public:
  explicit VulkanWindow(QWindow* parent = nullptr);

  void set_world(ecs::World* world) noexcept
  {
      m_world = world; 
  }
  [[nodiscard]] ecs::World* world() noexcept
  {
      return m_world; 
  }

  /// Host widget that owns the box-select overlay (above the Vulkan container).
  void set_rubber_band_host(QWidget* host) noexcept
  {
      m_rubberHost = host; 
  }

  /// When false, left-click will not change selection (e.g. interactive tool).
  void set_selection_enabled(bool enabled) noexcept
  {
    m_selectionEnabled = enabled;
  }

  using SelectionCallback = std::function<void(entt::entity)>;
  void set_selection_callback(SelectionCallback cb)
  {
    m_selectionCallback = std::move(cb);
  }

  /// Called for mouse moves while an interactive tool owns the viewport
  /// (`m_selectionEnabled == false`). Coordinates are in QWindow pixels.
  using ToolMotionCallback = std::function<void(float x, float y)>;
  void set_tool_motion_callback(ToolMotionCallback cb)
  {
    m_toolMotionCallback = std::move(cb);
  }

  /// Left-button press while a tool owns the viewport. Return true if consumed.
  using ToolPressCallback = std::function<bool(float x, float y, int button)>;
  void set_tool_press_callback(ToolPressCallback cb)
  {
    m_toolPressCallback = std::move(cb);
  }

  /// Right-click (no drag) in selection mode. Window-local x/y.
  using ContextMenuCallback = std::function<void(float x, float y)>;
  void set_context_menu_callback(ContextMenuCallback cb)
  {
    m_contextMenuCallback = std::move(cb);
  }

  [[nodiscard]] Camera& camera() noexcept
  {
      return m_camera; 
  }
  [[nodiscard]] const Camera& camera() const noexcept
  {
      return m_camera; 
  }

  QVulkanWindowRenderer* createRenderer() override;
  bool eventFilter(QObject* watched, QEvent* event) override;

  /// Apply a wheel zoom step (positive = zoom in). Used by overlays / MainWindow.
  void handle_wheel(int angle_delta_y)
  {
      pointer_wheel(angle_delta_y); 
  }

  void set_preview_edges(EdgeMesh edges);
  void set_preview(EdgeMesh edges, TriangleMesh solid);
  void clear_preview();
  void set_snap_overlay(EdgeMesh edges);
  void clear_snap_overlay();

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
  void maybe_box_select(float x0, float y0, float x1, float y1, bool multi);
  void update_rubber_band(float x0, float y0, float x1, float y1);
  void hide_rubber_band();
  void restore_idle_cursor();
  void fit_view_to_scene();
  [[nodiscard]] bool forward_tool_press(QPointF pos, Qt::MouseButton button);
  [[nodiscard]] QRect rubber_band_geometry(float x0, float y0, float x1,
                                           float y1) const;
  void begin_right_press(QPointF pos);
  [[nodiscard]] bool finish_right_release(QPointF pos);

  ecs::World* m_world{nullptr};
  Camera m_camera{};
  VulkanRenderer* m_renderer{nullptr};
  QWidget* m_rubberHost{nullptr};
  SelectRectOverlay* m_rubberBand{nullptr};  // child of m_rubberHost
  bool m_selectionEnabled{true};
  bool m_rightPressActive{false};
  bool m_rightMoved{false};
  float m_rightPressX{0.0f};
  float m_rightPressY{0.0f};
  SelectionCallback m_selectionCallback;
  ToolMotionCallback m_toolMotionCallback;
  ToolPressCallback m_toolPressCallback;
  ContextMenuCallback m_contextMenuCallback;
};

}  // namespace brep::viewer
