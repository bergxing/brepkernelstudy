#include "vulkan_window.hpp"

#include "ecs/systems.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace brep::viewer {

VulkanWindow::VulkanWindow(QWindow* parent) : QVulkanWindow(parent) {
  setTitle(QStringLiteral("brep-kernel viewer"));
  setKeyboardGrabEnabled(false);
}

Camera& VulkanWindow::camera() {
  if (world_) {
    if (Camera* cam = world_->main_camera()) return *cam;
  }
  return fallback_camera_;
}

const Camera& VulkanWindow::camera() const {
  if (world_) {
    if (const Camera* cam = world_->main_camera()) return *cam;
  }
  return fallback_camera_;
}

void VulkanWindow::sync_renderer() {
  if (!world_ || !renderer_) return;
  ecs::render_sync(world_->registry(), *renderer_);
}

void VulkanWindow::set_preview_edges(EdgeMesh edges) {
  if (!renderer_) return;
  renderer_->set_preview_edges(std::move(edges));
  requestUpdate();
}

void VulkanWindow::clear_preview() {
  if (!renderer_) return;
  renderer_->clear_preview();
  requestUpdate();
}

QVulkanWindowRenderer* VulkanWindow::createRenderer() {
  renderer_ = new VulkanRenderer(this);
  sync_renderer();
  return renderer_;
}

void VulkanWindow::maybe_select_at(float x, float y) {
  if (!selection_enabled_ || !world_) return;
  const QSize sz = size();
  const entt::entity hit = ecs::pick_renderable(
      world_->registry(), camera(), sz.width(), sz.height(), x, y);
  ecs::set_selection(world_->registry(), hit);
  requestUpdate();
  if (selection_callback_) selection_callback_(hit);
}

void VulkanWindow::pointer_press(QPointF pos, Qt::MouseButton button) {
  if (!world_) return;
  const int mode = ecs::input_on_press(world_->registry(), float(pos.x()),
                                       float(pos.y()), int(button));
  if (mode == static_cast<int>(ecs::InputState::DragMode::Orbit)) {
    setCursor(Qt::ClosedHandCursor);
  } else if (mode == static_cast<int>(ecs::InputState::DragMode::Pan)) {
    setCursor(Qt::SizeAllCursor);
  }
}

void VulkanWindow::pointer_move(QPointF pos, Qt::MouseButtons buttons) {
  if (!world_) return;
  auto& state = world_->registry().ctx().get<ecs::InputState>();
  const auto before = state.drag_mode;
  ecs::input_on_move(world_->registry(), float(pos.x()), float(pos.y()),
                     int(buttons));
  if (before == ecs::InputState::DragMode::PendingSelect &&
      state.drag_mode == ecs::InputState::DragMode::Orbit) {
    setCursor(Qt::ClosedHandCursor);
  }
  if (ecs::consume_camera_dirty(world_->registry())) {
    requestUpdate();
  }
}

void VulkanWindow::pointer_release(QPointF pos) {
  if (!world_) return;
  const bool click = ecs::input_on_release(world_->registry());
  unsetCursor();
  if (click) {
    maybe_select_at(float(pos.x()), float(pos.y()));
  }
}

void VulkanWindow::pointer_wheel(int angle_delta_y) {
  if (!world_) return;
  ecs::input_on_wheel(world_->registry(), angle_delta_y);
  if (ecs::consume_camera_dirty(world_->registry())) {
    requestUpdate();
  }
}

void VulkanWindow::apply_key(int key) {
  if (!world_) return;
  ecs::input_on_key(world_->registry(), key);
  if (ecs::consume_camera_dirty(world_->registry())) {
    requestUpdate();
  }
}

void VulkanWindow::mousePressEvent(QMouseEvent* event) {
  pointer_press(event->position(), event->button());
  event->accept();
}

void VulkanWindow::mouseReleaseEvent(QMouseEvent* event) {
  pointer_release(event->position());
  event->accept();
}

void VulkanWindow::mouseMoveEvent(QMouseEvent* event) {
  pointer_move(event->position(), event->buttons());
  event->accept();
}

void VulkanWindow::wheelEvent(QWheelEvent* event) {
  int dy = event->angleDelta().y();
  // Precision touchpads often report pixelDelta with angleDelta == 0.
  if (dy == 0) dy = event->pixelDelta().y();
  if (dy != 0) pointer_wheel(dy);
  event->accept();
}

void VulkanWindow::keyPressEvent(QKeyEvent* event) {
  apply_key(event->key());
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
      auto* e = static_cast<QMouseEvent*>(event);
      pointer_release(e->position());
      return true;
    }
    case QEvent::MouseMove: {
      auto* e = static_cast<QMouseEvent*>(event);
      pointer_move(e->position(), e->buttons());
      return true;
    }
    case QEvent::Wheel: {
      auto* e = static_cast<QWheelEvent*>(event);
      int dy = e->angleDelta().y();
      if (dy == 0) dy = e->pixelDelta().y();
      if (dy != 0) pointer_wheel(dy);
      return true;
    }
    case QEvent::KeyPress: {
      auto* e = static_cast<QKeyEvent*>(event);
      apply_key(e->key());
      return true;
    }
    default:
      break;
  }
  return QObject::eventFilter(watched, event);
}

}  // namespace brep::viewer
