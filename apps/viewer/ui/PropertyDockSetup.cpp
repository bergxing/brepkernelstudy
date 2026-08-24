#include "MainWindow.h"

#include "adapter/SceneAdapter.h"
#include "api/Core.h"
#include "api/Modeling.h"
#include "commands/DocumentHistory.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"
#include "PropertyPanel.h"

#include <QDockWidget>
#include <QMenuBar>
#include <QToolBar>

namespace brep::viewer
{

void MainWindow::setup_property_dock()
{
  m_propertyDock = new QDockWidget(tr("Properties"), this);
  m_propertyDock->setObjectName(QStringLiteral("PropertyDock"));
  m_propertyDock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                  Qt::RightDockWidgetArea);
  m_propertyPanel = new PropertyPanel(m_propertyDock);
  m_propertyDock->setWidget(m_propertyPanel);
  m_propertyDock->setMinimumWidth(240);
  addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);

  m_sceneAdapter.set_document(m_world.document());
  m_propertyPanel->set_adapter(&m_sceneAdapter);
  m_propertyPanel->set_params_changed_callback(
      [this](brep::feat::FeatureId /*id*/)
      {
        m_sceneAdapter.set_document(m_world.document());
        brep::Part* part = m_sceneAdapter.main_part();
        if (!part) return;
        const Material material = wood_albedo_path().isEmpty()
                                      ? Material{}
                                      : MakeWoodMaterial(
                                            wood_albedo_path().toStdString());
        m_world.sync_part_bodies(*part, material);
        request_all_views_update();
        m_document.mark_dirty();
        refresh_window_title();

        auto* history = &m_commandManager.history();
        const std::string wood = wood_albedo_path().toStdString();
        history->push(commands::DocumentHistory::Entry{
            .label = QStringLiteral("编辑参数"),
            .undo =
                [this, wood] {
                  m_sceneAdapter.set_document(m_world.document());
                  m_sceneAdapter.undo_feature();
                  if (auto* p = m_sceneAdapter.main_part())
                  {
                    Material mat =
                        wood.empty() ? Material{} : MakeWoodMaterial(wood);
                    m_world.sync_part_bodies(*p, std::move(mat));
                  }
                  request_all_views_update();
                  update_property_panel(
                      ecs::selected_entity(m_world.registry()));
                  refresh_edit_actions();
                },
            .redo =
                [this, wood] {
                  m_sceneAdapter.set_document(m_world.document());
                  m_sceneAdapter.redo_feature();
                  if (auto* p = m_sceneAdapter.main_part())
                  {
                    Material mat =
                        wood.empty() ? Material{} : MakeWoodMaterial(wood);
                    m_world.sync_part_bodies(*p, std::move(mat));
                  }
                  request_all_views_update();
                  update_property_panel(
                      ecs::selected_entity(m_world.registry()));
                  refresh_edit_actions();
                },
        });
        refresh_edit_actions();
        update_property_panel(ecs::selected_entity(m_world.registry()));
      });

  auto* view_menu = menuBar()->addMenu(tr("&View"));
  view_menu->setObjectName(QStringLiteral("menu_view"));
  view_menu->addAction(m_propertyDock->toggleViewAction());
  if (m_viewToolbar)
  {
    view_menu->addAction(m_viewToolbar->toggleViewAction());
  }
}

void MainWindow::update_property_panel(entt::entity entity)
{
  if (!m_propertyPanel) return;
  m_sceneAdapter.set_document(m_world.document());
  m_propertyPanel->set_adapter(&m_sceneAdapter);
  m_propertyPanel->show_entity(m_world.registry(), entity);
}

}  // namespace brep::viewer
