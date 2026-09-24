#include "MainWindow.h"

#include "api/Core.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"

#include <QAction>
#include <QCursor>
#include <QMenu>

#include <string_view>

namespace brep::viewer
{

void MainWindow::show_viewport_context_menu(VulkanWindow* window, float x,
                                            float y)
{
  if (!window) return;
  hide_cursor_tip();
  if (m_commandManager.has_active_tool())
  {
    BREP_INFO("viewport RMB -> tool context menu '{}' at ({:.1f},{:.1f})",
              m_commandManager.active_tool()
                  ? m_commandManager.active_tool()->Id()
                  : std::string_view{},
              x, y);
    auto ctx = make_command_context();
    if (m_commandManager.tool_context_menu(ctx, x, y))
    {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(m_world.registry()));
    }
    return;
  }

  auto& registry = m_world.registry();
  const QSize sz = window->size();
  const entt::entity hit = ecs::pick_renderable(
      registry, window->camera(), sz.width(), sz.height(), x, y);

  const bool on_object = hit != entt::null;
  if (on_object)
  {
    if (!registry.all_of<ecs::SelectedTag>(hit))
  {
      ecs::set_selection(registry, hit);
      update_property_panel(ecs::selected_entity(registry));
      request_all_views_update();
    }
  }

  hide_cursor_tip();

  QMenu menu(this);
  auto* act_copy = menu.addAction(tr("Copy"));
  auto* act_move = menu.addAction(tr("Move"));
  auto* act_delete = menu.addAction(tr("Delete"));
  auto* act_union = menu.addAction(tr("Boolean Union"));
  auto* act_subtract = menu.addAction(tr("Boolean Subtract"));
  auto* act_intersect = menu.addAction(tr("Boolean Intersect"));
  menu.addSeparator();
  auto* act_undo = menu.addAction(tr("Undo"));
  auto* act_redo = menu.addAction(tr("Redo"));
  act_delete->setEnabled(ecs::selected_count(registry) > 0);
  act_undo->setEnabled(m_commandManager.history().can_undo());
  act_redo->setEnabled(m_commandManager.history().can_redo());

  const QPoint menuPos = QCursor::pos();
  BREP_INFO("viewport context menu show at global=({},{}) onObject={}",
            menuPos.x(), menuPos.y(), on_object);
  QAction* chosen = menu.exec(menuPos);
  refresh_cursor_tip();
  if (!chosen)
  {
    BREP_INFO("viewport context menu dismissed");
    return;
  }
  BREP_INFO("viewport context menu chosen '{}'", chosen->text().toStdString());

  if (chosen == act_copy)
  {
    if (!on_object)
  {
      ecs::clear_selection(registry);
      update_property_panel(entt::null);
      request_all_views_update();
    }
    run_command("edit.copy");
    return;
  }
  if (chosen == act_move)
  {
    if (!on_object)
    {
      ecs::clear_selection(registry);
      update_property_panel(entt::null);
      request_all_views_update();
    }
    run_command("edit.move");
    return;
  }
  if (chosen == act_delete)
  {
    run_command("edit.delete");
    return;
  }
  if (chosen == act_union)
  {
    run_command("boolean.union");
    return;
  }
  if (chosen == act_subtract)
  {
    run_command("boolean.subtract");
    return;
  }
  if (chosen == act_intersect)
  {
    run_command("boolean.intersect");
    return;
  }
  if (chosen == act_undo)
  {
    run_command("edit.undo");
    return;
  }
  if (chosen == act_redo)
  {
    run_command("edit.redo");
  }
}

}  // namespace brep::viewer
