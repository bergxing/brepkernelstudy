#include "main_window.hpp"

#include "brep/brep.hpp"
#include "commands/document_history.hpp"
#include "ecs/components.hpp"
#include "ecs/systems.hpp"
#include "property_panel.hpp"

#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>

namespace brep::viewer {

void MainWindow::setup_property_dock() {
  property_dock_ = new QDockWidget(tr("Properties"), this);
  property_dock_->setObjectName(QStringLiteral("PropertyDock"));
  property_dock_->setAllowedAreas(Qt::LeftDockWidgetArea |
                                  Qt::RightDockWidgetArea);
  property_panel_ = new PropertyPanel(property_dock_);
  property_dock_->setWidget(property_panel_);
  property_dock_->setMinimumWidth(240);
  addDockWidget(Qt::RightDockWidgetArea, property_dock_);

  if (auto* part = world_.document() ? world_.document()->main_part()
                                     : nullptr) {
    property_panel_->set_part(part);
  }
  property_panel_->set_params_changed_callback(
      [this](brep::feat::FeatureId /*id*/) {
        brep::Part* part =
            world_.document() ? world_.document()->main_part() : nullptr;
        if (!part) return;
        const Material material = wood_albedo_path().isEmpty()
                                      ? Material{}
                                      : make_wood_material(
                                            wood_albedo_path().toStdString());
        world_.sync_part_bodies(*part, material);
        request_all_views_update();
        document_.mark_dirty();
        refresh_window_title();

        auto* history = &command_manager_.history();
        const std::string wood = wood_albedo_path().toStdString();
        history->push(commands::DocumentHistory::Entry{
            .label = QStringLiteral("编辑参数"),
            .undo =
                [this, wood] {
                  brep::Part* p = world_.document()
                                      ? world_.document()->main_part()
                                      : nullptr;
                  if (!p) return;
                  p->feature_history().undo(*p);
                  Material mat =
                      wood.empty() ? Material{} : make_wood_material(wood);
                  world_.sync_part_bodies(*p, std::move(mat));
                  request_all_views_update();
                  update_property_panel(
                      ecs::selected_entity(world_.registry()));
                  refresh_edit_actions();
                },
            .redo =
                [this, wood] {
                  brep::Part* p = world_.document()
                                      ? world_.document()->main_part()
                                      : nullptr;
                  if (!p) return;
                  p->feature_history().redo(*p);
                  Material mat =
                      wood.empty() ? Material{} : make_wood_material(wood);
                  world_.sync_part_bodies(*p, std::move(mat));
                  request_all_views_update();
                  update_property_panel(
                      ecs::selected_entity(world_.registry()));
                  refresh_edit_actions();
                },
        });
        refresh_edit_actions();
        update_property_panel(ecs::selected_entity(world_.registry()));
      });

  auto* view_menu = menuBar()->addMenu(tr("&View"));
  view_menu->setObjectName(QStringLiteral("menu_view"));
  view_menu->addAction(property_dock_->toggleViewAction());
  if (view_toolbar_) {
    view_menu->addAction(view_toolbar_->toggleViewAction());
  }
}

void MainWindow::update_property_panel(entt::entity entity) {
  if (!property_panel_) return;
  if (auto* part = world_.document() ? world_.document()->main_part()
                                     : nullptr) {
    property_panel_->set_part(part);
  }
  property_panel_->show_entity(world_.registry(), entity);
}

}  // namespace brep::viewer
