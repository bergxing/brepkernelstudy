#include "main_window.hpp"

#include "commands/command_palette.hpp"
#include "ecs/components.hpp"
#include "ecs/systems.hpp"

#include <QApplication>
#include <QDialog>
#include <QMessageBox>
#include <QStatusBar>

namespace brep::viewer {

commands::CommandContext MainWindow::make_command_context() {
  commands::CommandContext ctx;
  ctx.world = &world_;
  ctx.session = &document_;
  ctx.history = &command_manager_.history();
  ctx.parent_widget = this;
  ctx.wood_albedo_path = wood_albedo_path().toStdString();

  auto* vw = active_vulkan_window();
  auto* container = active_viewport_container();
  ctx.viewport = vw;
  ctx.view_camera = vw ? &vw->camera() : nullptr;
  ctx.snap_settings = &snap_settings_;
  ctx.snap_session = &snap_session_;

  if (vw && vw->width() > 0 && vw->height() > 0) {
    ctx.viewport_w = vw->width();
    ctx.viewport_h = vw->height();
  } else if (container) {
    ctx.viewport_w = std::max(1, container->width());
    ctx.viewport_h = std::max(1, container->height());
  }

  ctx.report_status = [this](const QString& msg) {
    statusBar()->showMessage(msg);
    refresh_cursor_tip();
  };
  ctx.request_redraw = [this] { request_all_views_update(); };
  ctx.after_document_reset = [this] {
    rebind_view_cube_camera();
    refresh_window_title();
    update_property_panel(entt::null);
    request_all_views_update();
  };
  ctx.refresh_ui = [this] {
    refresh_window_title();
    refresh_edit_actions();
  };
  ctx.set_preview_edges = [this](EdgeMesh edges) {
    if (auto* active = active_vulkan_window()) {
      active->set_preview_edges(std::move(edges));
    }
  };
  ctx.set_preview = [this](EdgeMesh edges, TriangleMesh solid) {
    if (auto* active = active_vulkan_window()) {
      active->set_preview(std::move(edges), std::move(solid));
    }
  };
  ctx.clear_preview = [this] {
    if (auto* active = active_vulkan_window()) {
      active->clear_preview();
    }
  };
  return ctx;
}

void MainWindow::sync_tool_ui() {
  const bool tool = command_manager_.has_active_tool();
  const bool allow_sel = command_manager_.active_tool_allows_selection();
  for (auto* window : view_windows_) {
    if (window) window->set_selection_enabled(!tool || allow_sel);
  }

  // Crosshair only while the tool owns picking (base / place), not during
  // object selection.
  const bool want_cross = tool && !allow_sel;
  if (want_cross && !tool_cursor_overridden_) {
    QApplication::setOverrideCursor(Qt::CrossCursor);
    tool_cursor_overridden_ = true;
  } else if (!want_cross && tool_cursor_overridden_) {
    QApplication::restoreOverrideCursor();
    tool_cursor_overridden_ = false;
  }
  refresh_cursor_tip();
}

commands::CommandResult MainWindow::run_command(std::string_view command_id) {
  auto ctx = make_command_context();
  auto result = command_manager_.run(command_id, ctx);
  if (result.status == commands::CommandStatus::Failed &&
      !result.message.isEmpty()) {
    QMessageBox::warning(this, tr("Command Failed"), result.message);
  }
  sync_tool_ui();
  refresh_edit_actions();
  update_property_panel(ecs::selected_entity(world_.registry()));
  if (result.succeeded() || command_manager_.has_active_tool()) {
    refresh_window_title();
  }
  return result;
}

void MainWindow::bind_action(QAction* action, const char* command_id) {
  action->setData(QString::fromUtf8(command_id));
  connect(action, &QAction::triggered, this, &MainWindow::on_run_command);
}

void MainWindow::on_run_command() {
  auto* action = qobject_cast<QAction*>(sender());
  if (!action) return;
  const QString id = action->data().toString();
  if (id.isEmpty()) return;
  run_command(id.toStdString());
}

void MainWindow::on_command_palette() {
  commands::CommandPalette palette(commands_, this);
  if (palette.exec() != QDialog::Accepted) return;
  const QString id = palette.selected_command_id();
  if (!id.isEmpty()) run_command(id.toStdString());
}

}  // namespace brep::viewer
