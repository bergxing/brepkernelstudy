#include "main_window.hpp"

#include "ecs/components.hpp"
#include "ecs/systems.hpp"

#include <QAction>
#include <QCursor>
#include <QMenu>

namespace brep::viewer {

void MainWindow::show_viewport_context_menu(VulkanWindow* window, float x,
                                            float y) {
  if (!window || command_manager_.has_active_tool()) return;

  auto& registry = world_.registry();
  const QSize sz = window->size();
  const entt::entity hit = ecs::pick_renderable(
      registry, window->camera(), sz.width(), sz.height(), x, y);

  const bool on_object = hit != entt::null;
  if (on_object) {
    if (!registry.all_of<ecs::SelectedTag>(hit)) {
      ecs::set_selection(registry, hit);
      update_property_panel(ecs::selected_entity(registry));
      request_all_views_update();
    }
  }

  hide_cursor_tip();

  QMenu menu(this);
  auto* act_copy = menu.addAction(tr("Copy"));
  menu.addSeparator();
  auto* act_undo = menu.addAction(tr("Undo"));
  auto* act_redo = menu.addAction(tr("Redo"));
  act_undo->setEnabled(command_manager_.history().can_undo());
  act_redo->setEnabled(command_manager_.history().can_redo());

  QAction* chosen = menu.exec(QCursor::pos());
  refresh_cursor_tip();
  if (!chosen) return;

  if (chosen == act_copy) {
    if (!on_object) {
      ecs::clear_selection(registry);
      update_property_panel(entt::null);
      request_all_views_update();
    }
    run_command("edit.copy");
    return;
  }
  if (chosen == act_undo) {
    run_command("edit.undo");
    return;
  }
  if (chosen == act_redo) {
    run_command("edit.redo");
  }
}

}  // namespace brep::viewer
