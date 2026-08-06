#pragma once

#include "brep/material.hpp"
#include "brep/mesh.hpp"
#include "camera.hpp"
#include "vulkan_renderer.hpp"

#include <QVulkanWindow>

namespace brep::viewer {

class VulkanWindow final : public QVulkanWindow {
  Q_OBJECT
 public:
  explicit VulkanWindow(QWindow* parent = nullptr);

  void set_meshes(TriangleMesh triangles, EdgeMesh edges);
  void set_material(Material material);
  [[nodiscard]] Camera& camera() noexcept { return camera_; }
  [[nodiscard]] const Camera& camera() const noexcept { return camera_; }

  QVulkanWindowRenderer* createRenderer() override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  enum class DragMode { None, Orbit, Pan };

  void pointer_press(QPointF pos, Qt::MouseButton button);
  void pointer_move(QPointF pos, Qt::MouseButtons buttons);
  void pointer_release();
  void pointer_wheel(int angle_delta_y);
  void apply_key_orbit(int key);

  Camera camera_{};
  TriangleMesh pending_triangles_{};
  EdgeMesh pending_edges_{};
  Material pending_material_{};
  bool has_pending_meshes_{false};
  bool has_pending_material_{false};
  QPointF last_pos_{};
  DragMode drag_mode_{DragMode::None};
  VulkanRenderer* renderer_{nullptr};
};

}  // namespace brep::viewer
