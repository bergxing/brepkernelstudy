#pragma once

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
  [[nodiscard]] Camera& camera() noexcept { return camera_; }
  [[nodiscard]] const Camera& camera() const noexcept { return camera_; }

  QVulkanWindowRenderer* createRenderer() override;

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  Camera camera_{};
  TriangleMesh pending_triangles_{};
  EdgeMesh pending_edges_{};
  bool has_pending_meshes_{false};
  QPoint last_pos_{};
  VulkanRenderer* renderer_{nullptr};
};

}  // namespace brep::viewer
