#include "VulkanWindow.h"

#include "ecs/Systems.h"
#include "SelectRectOverlay.h"

#include "api/Core.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>

namespace brep::viewer
{

VulkanWindow::VulkanWindow(QWindow* parent) : QVulkanWindow(parent)
{
  setTitle(QStringLiteral("brep-kernel viewer"));
  setKeyboardGrabEnabled(false);
  // Explicit arrow so we never inherit QMdiSubWindow's border resize cursor.
  setCursor(Qt::ArrowCursor);
}

void VulkanWindow::sync_renderer()
{
  if (!m_renderer) return;
  m_renderer->sync_from_world();
}

void VulkanWindow::set_preview_edges(EdgeMesh edges)
{
  set_preview(std::move(edges), {});
}

void VulkanWindow::set_preview(EdgeMesh edges, TriangleMesh solid)
{
  if (!m_renderer) return;
  m_renderer->set_preview(std::move(edges), std::move(solid));
  requestUpdate();
}

void VulkanWindow::clear_preview()
{
  if (!m_renderer) return;
  m_renderer->clear_preview();
  requestUpdate();
}

void VulkanWindow::set_snap_overlay(EdgeMesh edges)
{
  if (!m_renderer) return;
  m_renderer->set_snap_overlay(std::move(edges));
  requestUpdate();
}

void VulkanWindow::clear_snap_overlay()
{
  if (!m_renderer) return;
  m_renderer->clear_snap_overlay();
  requestUpdate();
}

QVulkanWindowRenderer* VulkanWindow::createRenderer()
{
  m_renderer = new VulkanRenderer(this);
  sync_renderer();
  return m_renderer;
}

QRect VulkanWindow::rubber_band_geometry(float x0, float y0, float x1,
                                         float y1) const
{
  if (!m_rubberHost) return {};
  const int cw = std::max(1, m_rubberHost->width());
  const int ch = std::max(1, m_rubberHost->height());
  const int vw = std::max(1, width());
  const int vh = std::max(1, height());
  const float sx = float(cw) / float(vw);
  const float sy = float(ch) / float(vh);
  const int left = int(std::floor(std::min(x0, x1) * sx));
  const int top = int(std::floor(std::min(y0, y1) * sy));
  const int right = int(std::ceil(std::max(x0, x1) * sx));
  const int bottom = int(std::ceil(std::max(y0, y1) * sy));
  return QRect(QPoint(left, top), QPoint(right, bottom)).normalized();
}

void VulkanWindow::update_rubber_band(float x0, float y0, float x1, float y1)
{
  if (!m_rubberHost) return;
  if (!m_rubberBand)
  {
    m_rubberBand = new SelectRectOverlay(m_rubberHost);
  }
  // Left→right = window (solid blue); right→left = crossing (dashed green).
  m_rubberBand->show_rect(rubber_band_geometry(x0, y0, x1, y1), x1 < x0);
}

void VulkanWindow::hide_rubber_band()
{
  if (m_rubberBand) m_rubberBand->hide_rect();
}

void VulkanWindow::maybe_select_at(float x, float y, bool multi)
{
  if (!m_selectionEnabled || !m_world) return;
  const QSize sz = size();
  const entt::entity hit = ecs::pick_renderable(
      m_world->registry(), m_camera, sz.width(), sz.height(), x, y);
  if (multi)
  {
    if (hit != entt::null)
  {
      ecs::toggle_selection(m_world->registry(), hit);
    }
  }
  else
  {
    ecs::set_selection(m_world->registry(), hit);
  }
  requestUpdate();
  if (m_selectionCallback)
  {
    m_selectionCallback(ecs::selected_entity(m_world->registry()));
  }
}

void VulkanWindow::maybe_box_select(float x0, float y0, float x1, float y1,
                                    bool multi)
{
  if (!m_selectionEnabled || !m_world) return;
  const QSize sz = size();
  const auto hits = ecs::pick_renderables_in_rect(
      m_world->registry(), m_camera, sz.width(), sz.height(), x0, y0, x1, y1);
  ecs::select_entities(m_world->registry(), hits, multi);
  requestUpdate();
  if (m_selectionCallback)
  {
    m_selectionCallback(ecs::selected_entity(m_world->registry()));
  }
}

bool VulkanWindow::forward_tool_press(QPointF pos, Qt::MouseButton button)
{
  if (m_selectionEnabled || button != Qt::LeftButton || !m_toolPressCallback)
{
    return false;
  }
  BREP_INFO("VulkanWindow tool press at ({:.1f},{:.1f}) size={}x{}", pos.x(),
            pos.y(), width(), height());
  return m_toolPressCallback(float(pos.x()), float(pos.y()), int(button));
}

void VulkanWindow::fit_view_to_scene()
{
  if (!m_world) return;
  const float aspect =
      float(std::max(1, width())) / float(std::max(1, height()));
  ecs::fit_camera_to_scene(m_world->registry(), m_camera, aspect);
  if (m_world->registry().ctx().contains<ecs::InputState>())
  {
    m_world->registry().ctx().get<ecs::InputState>().drag_mode =
        ecs::InputState::DragMode::None;
  }
  hide_rubber_band();
  restore_idle_cursor();
  requestUpdate();
}

void VulkanWindow::pointer_double_click(Qt::MouseButton button)
{
  // Zoom-to-fit: plain middle double-click (not Ctrl+Middle orbit).
  if (button != Qt::MiddleButton) return;
  fit_view_to_scene();
}

void VulkanWindow::begin_right_press(QPointF pos)
{
  m_rightPressActive = true;
  m_rightMoved = false;
  m_rightPressX = float(pos.x());
  m_rightPressY = float(pos.y());
}

bool VulkanWindow::finish_right_release(QPointF pos)
{
  if (!m_rightPressActive) return false;
  m_rightPressActive = false;
  if (m_rightMoved || !m_selectionEnabled || !m_contextMenuCallback)
  {
    return true;
  }
  constexpr float kSlop = 5.0f;
  const float dx = float(pos.x()) - m_rightPressX;
  const float dy = float(pos.y()) - m_rightPressY;
  if (dx * dx + dy * dy >= kSlop * kSlop) return true;
  m_contextMenuCallback(m_rightPressX, m_rightPressY);
  return true;
}

void VulkanWindow::pointer_press(QPointF pos, Qt::MouseButton button,
                                 Qt::KeyboardModifiers modifiers)
{
  if (button == Qt::RightButton)
{
    begin_right_press(pos);
    return;
  }

  if (forward_tool_press(pos, button)) return;

  if (!m_world) return;
  // Interactive tools own the left button (pick points); do not start
  // select/orbit, and do not replace the application pick cursor.
  if (!m_selectionEnabled && button == Qt::LeftButton)
  {
    BREP_WARN("VulkanWindow left press in tool mode but no tool_press_callback");
    return;
  }

  const int mode =
      ecs::input_on_press(m_world->registry(), float(pos.x()), float(pos.y()),
                          int(button), int(modifiers));
  if (mode == static_cast<int>(ecs::InputState::DragMode::Orbit))
  {
    setCursor(Qt::ClosedHandCursor);
  } else if (mode == static_cast<int>(ecs::InputState::DragMode::Pan))
  {
    setCursor(Qt::SizeAllCursor);
  }
}

void VulkanWindow::restore_idle_cursor()
{
  // Do not call unsetCursor(): with MDI, that inherits the subwindow's last
  // border resize shape (↔/↕) and leaves it stuck over the viewport.
  if (m_selectionEnabled)
  {
    setCursor(Qt::ArrowCursor);
  }
}

void VulkanWindow::pointer_move(QPointF pos, Qt::MouseButtons buttons)
{
  if (m_rightPressActive && (buttons & Qt::RightButton))
{
    constexpr float kSlop = 5.0f;
    const float dx = float(pos.x()) - m_rightPressX;
    const float dy = float(pos.y()) - m_rightPressY;
    if (dx * dx + dy * dy >= kSlop * kSlop) m_rightMoved = true;
  }

  // QWindow-direct moves (common with QWidget::createWindowContainer) must
  // still drive tool rubber-band previews.
  if (!m_selectionEnabled && m_toolMotionCallback)
  {
    m_toolMotionCallback(float(pos.x()), float(pos.y()));
  }

  if (!m_world) return;
  if (!m_selectionEnabled && (buttons & Qt::LeftButton) &&
      !(buttons & Qt::MiddleButton))
  {
    return;
  }

  auto& state = m_world->registry().ctx().get<ecs::InputState>();
  ecs::input_on_move(m_world->registry(), m_camera, float(pos.x()), float(pos.y()),
                     int(buttons));

  if (m_selectionEnabled &&
      state.drag_mode == ecs::InputState::DragMode::BoxSelect)
  {
    update_rubber_band(state.press_x, state.press_y, state.last_x,
                       state.last_y);
  }

  if (m_selectionEnabled &&
      state.drag_mode == ecs::InputState::DragMode::None &&
      buttons == Qt::NoButton)
      {
    restore_idle_cursor();
  }
  if (ecs::consume_camera_dirty(m_world->registry()))
  {
    requestUpdate();
  }
}

void VulkanWindow::pointer_release(QPointF pos)
{
  if (finish_right_release(pos)) return;

  if (!m_world) return;
  if (!m_selectionEnabled)
  {
    // Reset any stray drag state, but keep the tool crosshair (override cursor).
    (void)ecs::input_on_release(m_world->registry());
    hide_rubber_band();
    return;
  }

  auto& state = m_world->registry().ctx().get<ecs::InputState>();
  const bool multi = state.multi_select;
  const bool is_box = state.drag_mode == ecs::InputState::DragMode::BoxSelect;
  const float x0 = state.press_x;
  const float y0 = state.press_y;
  const bool click = ecs::input_on_release(m_world->registry());
  hide_rubber_band();
  restore_idle_cursor();

  if (is_box)
  {
    maybe_box_select(x0, y0, float(pos.x()), float(pos.y()), multi);
  } else if (click)
  {
    maybe_select_at(float(pos.x()), float(pos.y()), multi);
  }
}

void VulkanWindow::pointer_wheel(int angle_delta_y)
{
  if (!m_world) return;
  ecs::input_on_wheel(m_world->registry(), m_camera, angle_delta_y);
  if (ecs::consume_camera_dirty(m_world->registry()))
  {
    requestUpdate();
  }
}

void VulkanWindow::apply_key(int key)
{
  if (!m_world) return;
  ecs::input_on_key(m_world->registry(), m_camera, key);
  if (ecs::consume_camera_dirty(m_world->registry()))
  {
    requestUpdate();
  }
}

void VulkanWindow::mousePressEvent(QMouseEvent* event)
{
  pointer_press(event->position(), event->button(), event->modifiers());
  event->accept();
}

void VulkanWindow::mouseReleaseEvent(QMouseEvent* event)
{
  pointer_release(event->position());
  event->accept();
}

void VulkanWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
  // Ignore Ctrl+Middle double-click so it doesn't fight orbit.
  if (event->button() == Qt::MiddleButton &&
      (event->modifiers() & Qt::ControlModifier))
  {
    event->accept();
    return;
  }
  pointer_double_click(event->button());
  event->accept();
}

void VulkanWindow::mouseMoveEvent(QMouseEvent* event)
{
  pointer_move(event->position(), event->buttons());
  event->accept();
}

void VulkanWindow::wheelEvent(QWheelEvent* event)
{
  int dy = event->angleDelta().y();
  // Precision touchpads often report pixelDelta with angleDelta == 0.
  if (dy == 0) dy = event->pixelDelta().y();
  if (dy != 0) pointer_wheel(dy);
  event->accept();
}

void VulkanWindow::keyPressEvent(QKeyEvent* event)
{
  apply_key(event->key());
  event->accept();
}

bool VulkanWindow::eventFilter(QObject* watched, QEvent* event)
{
  Q_UNUSED(watched);
  switch (event->type())
  {
    case QEvent::Enter:
    case QEvent::HoverEnter:
      restore_idle_cursor();
      if (auto* w = qobject_cast<QWidget*>(watched))
      {
        w->setCursor(Qt::ArrowCursor);
      }
      break;
    case QEvent::MouseButtonDblClick: {
      auto* e = static_cast<QMouseEvent*>(event);
      if (e->button() == Qt::MiddleButton &&
          (e->modifiers() & Qt::ControlModifier))
      {
        return true;
      }
      pointer_double_click(e->button());
      return true;
    }
    case QEvent::MouseButtonPress: {
      auto* e = static_cast<QMouseEvent*>(event);
      pointer_press(e->position(), e->button(), e->modifiers());
      return true;
    }
    case QEvent::MouseButtonRelease: {
      auto* e = static_cast<QMouseEvent*>(event);
      if (e->button() == Qt::RightButton)
      {
        pointer_release(e->position());
        return true;
      }
      if (!m_selectionEnabled && e->button() == Qt::LeftButton)
      {
        return false;
      }
      pointer_release(e->position());
      return true;
    }
    case QEvent::MouseMove: {
      auto* e = static_cast<QMouseEvent*>(event);
      if (!m_selectionEnabled && (e->buttons() & Qt::LeftButton) &&
          !(e->buttons() & Qt::MiddleButton))
      {
        return false;
      }
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
