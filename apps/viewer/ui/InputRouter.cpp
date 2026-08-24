#include "MainWindow.h"

#include "commands/snap/Accusnap.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"

#include <QCursor>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QStatusBar>
#include <QWheelEvent>

#include <algorithm>

namespace brep::viewer
{
namespace
{

int wheel_delta_y(const QWheelEvent* event)
{
  if (event->angleDelta().y() != 0) return event->angleDelta().y();
  if (event->pixelDelta().y() != 0) return event->pixelDelta().y();
  return 0;
}

std::optional<SnapKind> snap_override_for_key(int key)
{
  switch (key)
{
    case Qt::Key_E:
      return SnapKind::Endpoint;
    case Qt::Key_M:
      return SnapKind::Midpoint;
    case Qt::Key_C:
      return SnapKind::Center;
    case Qt::Key_I:
      return SnapKind::Intersection;
    case Qt::Key_P:
      return SnapKind::Perpendicular;
    case Qt::Key_G:
      return SnapKind::Grid;
    default:
      return std::nullopt;
  }
}

}  // namespace

bool MainWindow::is_view_layout_object(const QObject* watched) const
{
  if (!watched || !m_mdiArea) return false;
  if (watched == m_mdiArea || watched == m_mdiArea->viewport()) return true;
  for (auto it = m_viewWindows.cbegin(); it != m_viewWindows.cend(); ++it)
  {
    if (watched == it.key() || watched == it.key()->widget()) return true;
  }
  return false;
}

void MainWindow::apply_wheel_zoom(VulkanWindow* window, int dy)
{
  if (!window || dy == 0) return;
  window->handle_wheel(dy);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
  if (handle_snap_key(event, true))
{
    event->accept();
    return;
  }
  if (m_commandManager.has_active_tool())
  {
    if (event->key() == Qt::Key_Escape)
  {
      auto ctx = make_command_context();
      m_commandManager.cancel_active_tool(ctx);
      sync_tool_ui();
      event->accept();
      return;
    }
    auto ctx = make_command_context();
    if (m_commandManager.tool_key_press(ctx, int(event->key())))
    {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(m_world.registry()));
      event->accept();
      return;
    }
  } else if (event->key() == Qt::Key_Delete ||
             event->key() == Qt::Key_Backspace)
  {
    if (ecs::selected_count(m_world.registry()) > 0)
  {
      run_command("edit.delete");
      event->accept();
      return;
    }
  }
  QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
  if (handle_snap_key(event, false))
{
    event->accept();
    return;
  }
  QMainWindow::keyReleaseEvent(event);
}

bool MainWindow::handle_snap_key(QKeyEvent* event, bool pressed)
{
  if (!event) return false;
  if (event->key() == Qt::Key_F3)
  {
    if (pressed && !event->isAutoRepeat())
  {
      set_snap_enabled(!m_snapSettings.enabled);
    }
    return true;
  }

  const auto override_kind = snap_override_for_key(event->key());
  if (!override_kind || !m_commandManager.has_active_tool()) return false;
  if (event->isAutoRepeat()) return true;

  const int key = event->key();
  std::erase(m_heldSnapOverrideKeys, key);
  if (pressed) m_heldSnapOverrideKeys.push_back(key);

  if (m_heldSnapOverrideKeys.empty())
  {
    m_snapSession.hold_override.reset();
  }
  else
  {
    m_snapSession.hold_override =
        snap_override_for_key(m_heldSnapOverrideKeys.back());
  }
  auto ctx = make_command_context();
  commands::AccuSnap::clear_feedback(ctx);
  request_all_views_update();
  return true;
}

bool MainWindow::handle_tool_mouse(QEvent* event)
{
  if (!m_commandManager.has_active_tool()) return false;
  // Selection phase (e.g. Copy): let the viewport handle pick / box / Ctrl.
  if (m_commandManager.active_tool_allows_selection()) return false;

  const QEvent::Type type = event->type();
  if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease &&
      type != QEvent::MouseMove)
  {
    return false;
  }

  auto* e = static_cast<QMouseEvent*>(event);
  const QPoint global = e->globalPosition().toPoint();
  auto* vw = vulkan_window_at_global(global);
  if (!vw) return false;

  // Activate the viewport under the cursor so picking uses its camera.
  for (auto it = m_viewWindows.begin(); it != m_viewWindows.end(); ++it)
  {
    if (it.value() == vw)
  {
      if (m_mdiArea->activeSubWindow() != it.key())
  {
        m_mdiArea->setActiveSubWindow(it.key());
      }
      break;
    }
  }

  QWidget* container = active_viewport_container();
  if (!container) return false;

  const QPoint local = container->mapFromGlobal(global);
  if (!container->rect().contains(local)) return false;

  const int cw = std::max(1, container->width());
  const int ch = std::max(1, container->height());
  const int vww = std::max(1, vw->width());
  const int vwh = std::max(1, vw->height());
  const float sx = float(local.x()) * float(vww) / float(cw);
  const float sy = float(local.y()) * float(vwh) / float(ch);

  auto ctx = make_command_context();
  if (type == QEvent::MouseButtonPress)
  {
    if (e->button() != Qt::LeftButton) return false;
    if (m_commandManager.tool_mouse_press(ctx, sx, sy, int(e->button())))
    {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(m_world.registry()));
      return true;
    }
    return false;
  }

  if (type == QEvent::MouseButtonRelease)
  {
    if (e->button() != Qt::LeftButton) return false;
    sync_tool_ui();
    return true;
  }

  m_commandManager.tool_mouse_move(ctx, sx, sy);
  if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) return false;
  return true;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
  if ((event->type() == QEvent::ApplicationDeactivate ||
       event->type() == QEvent::WindowDeactivate) &&
      !m_heldSnapOverrideKeys.empty())
       {
    m_heldSnapOverrideKeys.clear();
    m_snapSession.hold_override.reset();
    m_snapSession.active_snap.reset();
    for (auto* window : m_viewWindows)
    {
      if (window) window->clear_snap_overlay();
    }
    refresh_cursor_tip();
  }

  if (event->type() == QEvent::MouseMove ||
      event->type() == QEvent::HoverMove)
  {
    QPoint global;
    if (event->type() == QEvent::MouseMove)
    {
      global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
    }
    else
    {
      global = static_cast<QHoverEvent*>(event)->globalPosition().toPoint();
    }
    update_cursor_tip_at_global(global);
  } else if (event->type() == QEvent::Leave)
  {
    if (qobject_cast<QWidget*>(watched))
  {
      // Leaving a viewport container / MDI child — hide if cursor left views.
      if (!vulkan_window_at_global(QCursor::pos()))
      {
        hide_cursor_tip();
      }
    }
  }

  // Block closing the last MDI view subwindow.
  if (event->type() == QEvent::Close)
  {
    auto* sub = qobject_cast<QMdiSubWindow*>(watched);
    if (sub && m_mdiArea && m_viewWindows.contains(sub))
    {
      if (m_mdiArea->subWindowList().size() <= 1)
    {
        statusBar()->showMessage(QStringLiteral("至少保留一个视图窗口"), 3000);
        event->ignore();
        return true;
      }
    }
  }

  // Keep ViewCube glued to the active viewport when MDI windows move/resize.
  switch (event->type())
  {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::WindowStateChange:
    case QEvent::LayoutRequest:
      if (is_view_layout_object(watched))
    {
        place_view_cube();
      }
      break;
    default:
      break;
  }

  if (handle_tool_mouse(event)) return true;

  if (event->type() == QEvent::KeyPress)
  {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (handle_snap_key(ke, true)) return true;
    if (m_commandManager.has_active_tool())
    {
      if (ke->key() == Qt::Key_Escape)
    {
        auto ctx = make_command_context();
        m_commandManager.cancel_active_tool(ctx);
        sync_tool_ui();
        return true;
      }
      auto ctx = make_command_context();
      if (m_commandManager.tool_key_press(ctx, int(ke->key())))
      {
        sync_tool_ui();
        refresh_edit_actions();
        update_property_panel(ecs::selected_entity(m_world.registry()));
        return true;
      }
    } else if (ke->key() == Qt::Key_Delete ||
               ke->key() == Qt::Key_Backspace)
    {
      if (ecs::selected_count(m_world.registry()) > 0)
    {
        run_command("edit.delete");
        return true;
      }
    }
  } else if (event->type() == QEvent::KeyRelease)
  {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (handle_snap_key(ke, false)) return true;
  }

  if (event->type() == QEvent::Wheel)
  {
    auto* we = static_cast<QWheelEvent*>(event);
    const QPoint global = we->globalPosition().toPoint();
    if (m_viewCube && m_viewCube->isVisible())
    {
      const QRect cube_global(m_viewCube->pos(), m_viewCube->size());
      if (cube_global.contains(global))
      {
        return QMainWindow::eventFilter(watched, event);
      }
    }
    if (auto* vw = vulkan_window_at_global(global))
    {
      const int dy = wheel_delta_y(we);
      if (dy != 0)
      {
        apply_wheel_zoom(vw, dy);
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
