#pragma once

#include "ecs/World.h"
#include "VulkanRenderer.h"

#include "api/Core.h"
#include "api/Modeling.h"

#include <QVulkanWindow>

#include <functional>

class QWidget;

namespace brep::viewer
{

namespace adapter
{
class ISceneService;
class ISceneServiceFactory;
}

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

  void set_scene_service(adapter::ISceneService* scene) noexcept
  {
      m_sceneService = scene; 
  }
  void set_scene_factory(adapter::ISceneServiceFactory* factory) noexcept
  {
      m_sceneFactory = factory; 
  }
  [[nodiscard]] adapter::ISceneService* scene_service() const noexcept
  {
      return m_sceneService; 
  }
  [[nodiscard]] adapter::ISceneServiceFactory* scene_factory() const noexcept
  {
      return m_sceneFactory; 
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

  /// AccuSnap pick for Bezier CV drag (selection mode).
  using SnapPickCallback = std::function<bool(float x, float y, Point3d& hit)>;
  void set_snap_pick_callback(SnapPickCallback cb)
  {
    m_snapPickCallback = std::move(cb);
  }

  /// Persist Bezier CV edit (SetPrimitive + history + mesh sync).
  using BezierEditCommitCallback = std::function<void(
      Guid featureGuid, Guid bodyGuid, const BezierSpec& before,
      const BezierSpec& after)>;
  void set_bezier_edit_commit_callback(BezierEditCommitCallback cb)
  {
    m_bezierEditCommitCallback = std::move(cb);
  }

  /// Persist NurbsCurve CV edit (SetPrimitive + history + mesh sync).
  using NurbsEditCommitCallback = std::function<void(
      Guid featureGuid, Guid bodyGuid, const NurbsCurveSpec& before,
      const NurbsCurveSpec& after)>;
  void set_nurbs_edit_commit_callback(NurbsEditCommitCallback cb)
  {
    m_nurbsEditCommitCallback = std::move(cb);
  }

  using StatusMessageCallback = std::function<void(const QString& msg)>;
  void set_status_message_callback(StatusMessageCallback cb)
  {
    m_statusMessageCallback = std::move(cb);
  }

  void CancelBezierCvDrag();
  [[nodiscard]] bool BezierCvDragActive() const noexcept
  {
    return m_bezierDrag.Active;
  }

  void CancelNurbsCvDrag();
  [[nodiscard]] bool NurbsCvDragActive() const noexcept
  {
    return m_nurbsDrag.Active;
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
  void set_viewport_colors(float clearR, float clearG, float clearB,
                           float wireR, float wireG, float wireB,
                           float hoverR, float hoverG, float hoverB,
                           float previewR, float previewG, float previewB);

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

  [[nodiscard]] bool TryBeginBezierCvDrag(float x, float y);
  void UpdateBezierCvDrag(float x, float y);
  void FinishBezierCvDrag(bool commit);

  [[nodiscard]] bool TryBeginNurbsCvDrag(float x, float y);
  void UpdateNurbsCvDrag(float x, float y);
  void FinishNurbsCvDrag(bool commit);

  ecs::World* m_world{nullptr};
  adapter::ISceneService* m_sceneService{nullptr};
  adapter::ISceneServiceFactory* m_sceneFactory{nullptr};
  Camera m_camera{};
  VulkanRenderer* m_renderer{nullptr};
  float m_themeClear[3]{0.12f, 0.13f, 0.15f};
  float m_themeWire[3]{0.78f, 0.80f, 0.84f};
  float m_themeHover[3]{0.25f, 0.85f, 1.0f};
  float m_themePreview[3]{1.0f, 0.92f, 0.15f};
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
  SnapPickCallback m_snapPickCallback;
  BezierEditCommitCallback m_bezierEditCommitCallback;
  NurbsEditCommitCallback m_nurbsEditCommitCallback;
  StatusMessageCallback m_statusMessageCallback;

  struct BezierDragState
  {
    bool Active{false};
    entt::entity Entity{entt::null};
    int CvIndex{-1};
    BezierSpec Spec{};
    BezierSpec SpecAtPress{};
    Guid FeatureGuid{};
    Guid BodyGuid{};
  };
  BezierDragState m_bezierDrag;

  struct NurbsDragState
  {
    bool Active{false};
    entt::entity Entity{entt::null};
    int CvIndex{-1};
    NurbsCurveSpec Spec{};
    NurbsCurveSpec SpecAtPress{};
    Guid FeatureGuid{};
    Guid BodyGuid{};
  };
  NurbsDragState m_nurbsDrag;
};

}  // namespace brep::viewer
