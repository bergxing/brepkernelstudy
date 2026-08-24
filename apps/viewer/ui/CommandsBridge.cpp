#include "MainWindow.h"

#include "commands/CommandPalette.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"

#include <QApplication>
#include <QDialog>
#include <QMessageBox>
#include <QStatusBar>

namespace brep::viewer
{

commands::CommandContext MainWindow::make_command_context()
{
  commands::CommandContext ctx;
  ctx.World = &m_world;
  ctx.Session = &m_document;
  ctx.History = &m_commandManager.history();
  ctx.ParentWidget = this;
  ctx.WoodAlbedoPath = wood_albedo_path().toStdString();

  auto* vw = active_vulkan_window();
  auto* container = active_viewport_container();
  ctx.Viewport = vw;
  ctx.ViewCamera = vw ? &vw->camera() : nullptr;
  ctx.SnapSettingsRef = &m_snapSettings;
  ctx.SnapSessionRef = &m_snapSession;

  if (vw && vw->width() > 0 && vw->height() > 0)
  {
    ctx.ViewportWidth = vw->width();
    ctx.ViewportHeight = vw->height();
  } else if (container)
  {
    ctx.ViewportWidth = std::max(1, container->width());
    ctx.ViewportHeight = std::max(1, container->height());
  }

  ctx.ReportStatus = [this](const QString& msg)
  {
    statusBar()->showMessage(msg);
    refresh_cursor_tip();
  };
  ctx.RequestRedraw = [this] { request_all_views_update(); };
  ctx.AfterDocumentReset = [this] {
    rebind_view_cube_camera();
    refresh_window_title();
    update_property_panel(entt::null);
    request_all_views_update();
  };
  ctx.RefreshUi = [this] {
    refresh_window_title();
    refresh_edit_actions();
  };
  ctx.SetPreviewEdges = [this](EdgeMesh edges)
  {
    if (auto* active = active_vulkan_window())
  {
      active->set_preview_edges(std::move(edges));
    }
  };
  ctx.SetPreview = [this](EdgeMesh edges, TriangleMesh solid)
  {
    if (auto* active = active_vulkan_window())
  {
      active->set_preview(std::move(edges), std::move(solid));
    }
  };
  ctx.ClearPreview = [this] {
    if (auto* active = active_vulkan_window())
  {
      active->clear_preview();
    }
  };
  ctx.SetSnapOverlay = [this](EdgeMesh edges)
  {
    if (auto* active = active_vulkan_window())
  {
      active->set_snap_overlay(std::move(edges));
    }
  };
  ctx.ClearSnapOverlay = [this] {
    if (auto* active = active_vulkan_window())
  {
      active->clear_snap_overlay();
    }
  };
  ctx.RefreshCursorTip = [this] { refresh_cursor_tip(); };
  return ctx;
}

void MainWindow::sync_tool_ui()
{
  const bool tool = m_commandManager.has_active_tool();
  const bool allow_sel = m_commandManager.active_tool_allows_selection();
  if (!tool)
  {
    m_heldSnapOverrideKeys.clear();
    m_snapSession.hold_override.reset();
  }
  for (auto* window : m_viewWindows)
  {
    if (window) window->set_selection_enabled(!tool || allow_sel);
  }
  if (!tool || allow_sel)
  {
    m_snapSession.active_snap.reset();
    for (auto* window : m_viewWindows)
    {
      if (window) window->clear_snap_overlay();
    }
  }

  // Crosshair only while the tool owns picking (base / place), not during
  // object selection.
  const bool want_cross = tool && !allow_sel;
  if (want_cross && !m_toolCursorOverridden)
  {
    QApplication::setOverrideCursor(Qt::CrossCursor);
    m_toolCursorOverridden = true;
  } else if (!want_cross && m_toolCursorOverridden)
  {
    QApplication::restoreOverrideCursor();
    m_toolCursorOverridden = false;
  }
  refresh_cursor_tip();
}

commands::CommandResult MainWindow::run_command(std::string_view command_id)
{
  auto ctx = make_command_context();
  auto result = m_commandManager.run(command_id, ctx);
  if (result.Status == commands::CommandStatus::Failed &&
      !result.Message.isEmpty())
  {
    QMessageBox::warning(this, tr("Command Failed"), result.Message);
  }
  sync_tool_ui();
  refresh_edit_actions();
  update_property_panel(ecs::selected_entity(m_world.registry()));
  if (result.Succeeded() || m_commandManager.has_active_tool())
  {
    refresh_window_title();
  }
  return result;
}

void MainWindow::bind_action(QAction* action, const char* command_id)
{
  action->setData(QString::fromUtf8(command_id));
  connect(action, &QAction::triggered, this, &MainWindow::on_run_command);
}

void MainWindow::on_run_command()
{
  auto* action = qobject_cast<QAction*>(sender());
  if (!action) return;
  const QString id = action->data().toString();
  if (id.isEmpty()) return;
  run_command(id.toStdString());
}

void MainWindow::on_command_palette()
{
  commands::CommandPalette palette(m_commands, this);
  if (palette.exec() != QDialog::Accepted) return;
  const QString id = palette.selected_command_id();
  if (!id.isEmpty()) run_command(id.toStdString());
}

}  // namespace brep::viewer
