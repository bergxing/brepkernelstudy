#include "main_window.hpp"

#include "commands/snap/accusnap.hpp"
#include "ecs/components.hpp"
#include "ecs/systems.hpp"

#include <QCursor>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QStatusBar>
#include <QWheelEvent>

#include <algorithm>

namespace brep::viewer {
namespace {

int wheel_delta_y(const QWheelEvent* event) {
  if (event->angleDelta().y() != 0) return event->angleDelta().y();
  if (event->pixelDelta().y() != 0) return event->pixelDelta().y();
  return 0;
}

std::optional<SnapKind> snap_override_for_key(int key) {
  switch (key) {
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

bool MainWindow::is_view_layout_object(const QObject* watched) const {
  if (!watched || !mdi_area_) return false;
  if (watched == mdi_area_ || watched == mdi_area_->viewport()) return true;
  for (auto it = view_windows_.cbegin(); it != view_windows_.cend(); ++it) {
    if (watched == it.key() || watched == it.key()->widget()) return true;
  }
  return false;
}

void MainWindow::apply_wheel_zoom(VulkanWindow* window, int dy) {
  if (!window || dy == 0) return;
  window->handle_wheel(dy);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
  if (handle_snap_key(event, true)) {
    event->accept();
    return;
  }
  if (command_manager_.has_active_tool()) {
    if (event->key() == Qt::Key_Escape) {
      auto ctx = make_command_context();
      command_manager_.cancel_active_tool(ctx);
      sync_tool_ui();
      event->accept();
      return;
    }
    auto ctx = make_command_context();
    if (command_manager_.tool_key_press(ctx, int(event->key()))) {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(world_.registry()));
      event->accept();
      return;
    }
  } else if (event->key() == Qt::Key_Delete ||
             event->key() == Qt::Key_Backspace) {
    if (ecs::selected_count(world_.registry()) > 0) {
      run_command("edit.delete");
      event->accept();
      return;
    }
  }
  QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
  if (handle_snap_key(event, false)) {
    event->accept();
    return;
  }
  QMainWindow::keyReleaseEvent(event);
}

bool MainWindow::handle_snap_key(QKeyEvent* event, bool pressed) {
  if (!event) return false;
  if (event->key() == Qt::Key_F3) {
    if (pressed && !event->isAutoRepeat()) {
      set_snap_enabled(!snap_settings_.enabled);
    }
    return true;
  }

  const auto override_kind = snap_override_for_key(event->key());
  if (!override_kind || !command_manager_.has_active_tool()) return false;
  if (event->isAutoRepeat()) return true;

  const int key = event->key();
  std::erase(held_snap_override_keys_, key);
  if (pressed) held_snap_override_keys_.push_back(key);

  if (held_snap_override_keys_.empty()) {
    snap_session_.hold_override.reset();
  } else {
    snap_session_.hold_override =
        snap_override_for_key(held_snap_override_keys_.back());
  }
  auto ctx = make_command_context();
  commands::AccuSnap::clear_feedback(ctx);
  request_all_views_update();
  return true;
}

bool MainWindow::handle_tool_mouse(QEvent* event) {
  if (!command_manager_.has_active_tool()) return false;
  // Selection phase (e.g. Copy): let the viewport handle pick / box / Ctrl.
  if (command_manager_.active_tool_allows_selection()) return false;

  const QEvent::Type type = event->type();
  if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease &&
      type != QEvent::MouseMove) {
    return false;
  }

  auto* e = static_cast<QMouseEvent*>(event);
  const QPoint global = e->globalPosition().toPoint();
  auto* vw = vulkan_window_at_global(global);
  if (!vw) return false;

  // Activate the viewport under the cursor so picking uses its camera.
  for (auto it = view_windows_.begin(); it != view_windows_.end(); ++it) {
    if (it.value() == vw) {
      if (mdi_area_->activeSubWindow() != it.key()) {
        mdi_area_->setActiveSubWindow(it.key());
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
  if (type == QEvent::MouseButtonPress) {
    if (e->button() != Qt::LeftButton) return false;
    if (command_manager_.tool_mouse_press(ctx, sx, sy, int(e->button()))) {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(world_.registry()));
      return true;
    }
    return false;
  }

  if (type == QEvent::MouseButtonRelease) {
    if (e->button() != Qt::LeftButton) return false;
    sync_tool_ui();
    return true;
  }

  command_manager_.tool_mouse_move(ctx, sx, sy);
  if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) return false;
  return true;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if ((event->type() == QEvent::ApplicationDeactivate ||
       event->type() == QEvent::WindowDeactivate) &&
      !held_snap_override_keys_.empty()) {
    held_snap_override_keys_.clear();
    snap_session_.hold_override.reset();
    snap_session_.active_snap.reset();
    for (auto* window : view_windows_) {
      if (window) window->clear_snap_overlay();
    }
    refresh_cursor_tip();
  }

  if (event->type() == QEvent::MouseMove ||
      event->type() == QEvent::HoverMove) {
    QPoint global;
    if (event->type() == QEvent::MouseMove) {
      global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
    } else {
      global = static_cast<QHoverEvent*>(event)->globalPosition().toPoint();
    }
    update_cursor_tip_at_global(global);
  } else if (event->type() == QEvent::Leave) {
    if (qobject_cast<QWidget*>(watched)) {
      // Leaving a viewport container / MDI child — hide if cursor left views.
      if (!vulkan_window_at_global(QCursor::pos())) {
        hide_cursor_tip();
      }
    }
  }

  // Block closing the last MDI view subwindow.
  if (event->type() == QEvent::Close) {
    auto* sub = qobject_cast<QMdiSubWindow*>(watched);
    if (sub && mdi_area_ && view_windows_.contains(sub)) {
      if (mdi_area_->subWindowList().size() <= 1) {
        statusBar()->showMessage(QStringLiteral("至少保留一个视图窗口"), 3000);
        event->ignore();
        return true;
      }
    }
  }

  // Keep ViewCube glued to the active viewport when MDI windows move/resize.
  switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::WindowStateChange:
    case QEvent::LayoutRequest:
      if (is_view_layout_object(watched)) {
        place_view_cube();
      }
      break;
    default:
      break;
  }

  if (handle_tool_mouse(event)) return true;

  if (event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (handle_snap_key(ke, true)) return true;
    if (command_manager_.has_active_tool()) {
      if (ke->key() == Qt::Key_Escape) {
        auto ctx = make_command_context();
        command_manager_.cancel_active_tool(ctx);
        sync_tool_ui();
        return true;
      }
      auto ctx = make_command_context();
      if (command_manager_.tool_key_press(ctx, int(ke->key()))) {
        sync_tool_ui();
        refresh_edit_actions();
        update_property_panel(ecs::selected_entity(world_.registry()));
        return true;
      }
    } else if (ke->key() == Qt::Key_Delete ||
               ke->key() == Qt::Key_Backspace) {
      if (ecs::selected_count(world_.registry()) > 0) {
        run_command("edit.delete");
        return true;
      }
    }
  } else if (event->type() == QEvent::KeyRelease) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (handle_snap_key(ke, false)) return true;
  }

  if (event->type() == QEvent::Wheel) {
    auto* we = static_cast<QWheelEvent*>(event);
    const QPoint global = we->globalPosition().toPoint();
    if (view_cube_ && view_cube_->isVisible()) {
      const QRect cube_global(view_cube_->pos(), view_cube_->size());
      if (cube_global.contains(global)) {
        return QMainWindow::eventFilter(watched, event);
      }
    }
    if (auto* vw = vulkan_window_at_global(global)) {
      const int dy = wheel_delta_y(we);
      if (dy != 0) {
        apply_wheel_zoom(vw, dy);
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
