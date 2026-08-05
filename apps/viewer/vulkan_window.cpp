#include "vulkan_window.hpp"

#include <QMouseEvent>
#include <QWheelEvent>

namespace brep::viewer {

VulkanWindow::VulkanWindow(QWindow* parent) : QVulkanWindow(parent) {
  setTitle(QStringLiteral("brep-kernel viewer"));
}

void VulkanWindow::set_meshes(TriangleMesh triangles, EdgeMesh edges) {
  pending_triangles_ = std::move(triangles);
  pending_edges_ = std::move(edges);
  has_pending_meshes_ = true;
  if (renderer_) {
    renderer_->set_meshes(pending_triangles_, pending_edges_);
  }
}

QVulkanWindowRenderer* VulkanWindow::createRenderer() {
  renderer_ = new VulkanRenderer(this);
  if (has_pending_meshes_) {
    renderer_->set_meshes(pending_triangles_, pending_edges_);
  }
  return renderer_;
}

void VulkanWindow::mousePressEvent(QMouseEvent* event) {
  last_pos_ = event->position().toPoint();
}

void VulkanWindow::mouseMoveEvent(QMouseEvent* event) {
  if (event->buttons() & Qt::LeftButton) {
    const QPoint delta = event->position().toPoint() - last_pos_;
    camera_.orbit(float(delta.x()), float(delta.y()));
    last_pos_ = event->position().toPoint();
    requestUpdate();
  }
}

void VulkanWindow::wheelEvent(QWheelEvent* event) {
  camera_.zoom(event->angleDelta().y() > 0 ? 1.0f : -1.0f);
  requestUpdate();
}

}  // namespace brep::viewer
