#include "main_window.hpp"

#include "adapter/scene_adapter.hpp"
#include "brep/material.hpp"
#include "brep/part.hpp"
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

  scene_adapter_.set_document(world_.document());
  property_panel_->set_adapter(&scene_adapter_);
  property_panel_->set_params_changed_callback(
      [this](brep::feat::FeatureId /*id*/) {
        scene_adapter_.set_document(world_.document());
        brep::Part* part = scene_adapter_.main_part();
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
                  scene_adapter_.set_document(world_.document());
                  scene_adapter_.undo_feature();
                  if (auto* p = scene_adapter_.main_part()) {
                    Material mat =
                        wood.empty() ? Material{} : make_wood_material(wood);
                    world_.sync_part_bodies(*p, std::move(mat));
                  }
                  request_all_views_update();
                  update_property_panel(
                      ecs::selected_entity(world_.registry()));
                  refresh_edit_actions();
                },
            .redo =
                [this, wood] {
                  scene_adapter_.set_document(world_.document());
                  scene_adapter_.redo_feature();
                  if (auto* p = scene_adapter_.main_part()) {
                    Material mat =
                        wood.empty() ? Material{} : make_wood_material(wood);
                    world_.sync_part_bodies(*p, std::move(mat));
                  }
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
  scene_adapter_.set_document(world_.document());
  property_panel_->set_adapter(&scene_adapter_);
  property_panel_->show_entity(world_.registry(), entity);
}

}  // namespace brep::viewer
