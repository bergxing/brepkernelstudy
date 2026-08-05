#include "vulkan_window.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace brep::viewer {

VulkanWindow::VulkanWindow(QWindow* parent) : QVulkanWindow(parent) {
  setTitle(QStringLiteral("brep-kernel viewer"));
  setKeyboardGrabEnabled(false);
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

void VulkanWindow::pointer_press(QPointF pos, Qt::MouseButton button) {
  last_pos_ = pos;
  if (button == Qt::LeftButton) {
    drag_mode_ = DragMode::Orbit;
    setCursor(Qt::ClosedHandCursor);
  } else if (button == Qt::RightButton || button == Qt::MiddleButton) {
    drag_mode_ = DragMode::Pan;
    setCursor(Qt::SizeAllCursor);
  } else {
    drag_mode_ = DragMode::None;
  }
}

void VulkanWindow::pointer_move(QPointF pos, Qt::MouseButtons buttons) {
  if (drag_mode_ == DragMode::None) return;

  // Re-derive mode from buttons in case press was missed on one delivery path.
  if (buttons & Qt::LeftButton) {
    drag_mode_ = DragMode::Orbit;
  } else if (buttons & (Qt::RightButton | Qt::MiddleButton)) {
    drag_mode_ = DragMode::Pan;
  } else {
    return;
  }

  const QPointF delta = pos - last_pos_;
  if (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y())) return;

  if (drag_mode_ == DragMode::Orbit) {
    camera_.orbit(float(delta.x()), float(delta.y()));
  } else {
    camera_.pan(float(delta.x()), float(delta.y()));
  }
  last_pos_ = pos;
  requestUpdate();
}

void VulkanWindow::pointer_release() {
  drag_mode_ = DragMode::None;
  unsetCursor();
}

void VulkanWindow::pointer_wheel(int angle_delta_y) {
  if (angle_delta_y == 0) return;
  camera_.zoom(angle_delta_y > 0 ? 1.0f : -1.0f);
  requestUpdate();
}

void VulkanWindow::apply_key_orbit(int key) {
  constexpr float step = 8.0f;
  switch (key) {
    case Qt::Key_Left:
      camera_.orbit(-step, 0.0f);
      break;
    case Qt::Key_Right:
      camera_.orbit(step, 0.0f);
      break;
    case Qt::Key_Up:
      camera_.orbit(0.0f, -step);
      break;
    case Qt::Key_Down:
      camera_.orbit(0.0f, step);
      break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      camera_.zoom(1.0f);
      break;
    case Qt::Key_Minus:
      camera_.zoom(-1.0f);
      break;
    default:
      return;
  }
  requestUpdate();
}

void VulkanWindow::mousePressEvent(QMouseEvent* event) {
  pointer_press(event->position(), event->button());
  event->accept();
}

void VulkanWindow::mouseReleaseEvent(QMouseEvent* event) {
  pointer_release();
  event->accept();
}

void VulkanWindow::mouseMoveEvent(QMouseEvent* event) {
  pointer_move(event->position(), event->buttons());
  event->accept();
}

void VulkanWindow::wheelEvent(QWheelEvent* event) {
  pointer_wheel(event->angleDelta().y());
  event->accept();
}

void VulkanWindow::keyPressEvent(QKeyEvent* event) {
  apply_key_orbit(event->key());
  event->accept();
}

bool VulkanWindow::eventFilter(QObject* watched, QEvent* event) {
  Q_UNUSED(watched);
  switch (event->type()) {
    case QEvent::MouseButtonPress: {
      auto* e = static_cast<QMouseEvent*>(event);
      pointer_press(e->position(), e->button());
      return true;
    }
    case QEvent::MouseButtonRelease: {
      pointer_release();
      return true;
    }
    case QEvent::MouseMove: {
      auto* e = static_cast<QMouseEvent*>(event);
      pointer_move(e->position(), e->buttons());
      return true;
    }
    case QEvent::Wheel: {
      auto* e = static_cast<QWheelEvent*>(event);
      pointer_wheel(e->angleDelta().y());
      return true;
    }
    case QEvent::KeyPress: {
      auto* e = static_cast<QKeyEvent*>(event);
      apply_key_orbit(e->key());
      return true;
    }
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}

}  // namespace brep::viewer
