#include "MainWindow.h"

#include "adapter/ISceneService.h"
#include "api/Core.h"
#include "api/Modeling.h"
#include "commands/DocumentHistory.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"
#include "PropertyPanel.h"

#include <QDockWidget>
#include <QMenu>

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

  if (m_documentScope)
  {
    m_propertyPanel->SetAdapter(&m_documentScope->scene());
  }
  m_propertyPanel->SetParamsChangedCallback(
      [this](brep::feat::FeatureId /*id*/)
      {
        if (!m_documentScope)
        {
          return;
        }
        adapter::ISceneService& scene = m_documentScope->scene();
        brep::Part* part = scene.MainPart();
        if (!part) return;
        const Material material = wood_albedo_path().isEmpty()
                                      ? Material{}
                                      : MakeWoodMaterial(
                                            wood_albedo_path().toStdString());
        m_world.SyncPartBodies(*part, material);
        request_all_views_update();
        m_document.MarkDirty();
        refresh_window_title();

        auto* history = &m_commandManager.history();
        const std::string wood = wood_albedo_path().toStdString();
        history->push(commands::DocumentHistory::Entry{
            .label = QStringLiteral("编辑参数"),
            .undo =
                [this, wood] {
                  if (!m_documentScope)
                  {
                    return;
                  }
                  adapter::ISceneService& scene = m_documentScope->scene();
                  scene.UndoFeature();
                  if (auto* p = scene.MainPart())
                  {
                    Material mat =
                        wood.empty() ? Material{} : MakeWoodMaterial(wood);
                    m_world.SyncPartBodies(*p, std::move(mat));
                  }
                  request_all_views_update();
                  update_property_panel(
                      ecs::selected_entity(m_world.registry()));
                  refresh_edit_actions();
                },
            .redo =
                [this, wood] {
                  if (!m_documentScope)
                  {
                    return;
                  }
                  adapter::ISceneService& scene = m_documentScope->scene();
                  scene.RedoFeature();
                  if (auto* p = scene.MainPart())
                  {
                    Material mat =
                        wood.empty() ? Material{} : MakeWoodMaterial(wood);
                    m_world.SyncPartBodies(*p, std::move(mat));
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

  auto* view_menu = new QMenu(tr("&View"), this);
  view_menu->setObjectName(QStringLiteral("menu_view"));
  view_menu->addAction(m_propertyDock->toggleViewAction());
  if (auto* file_menu = findChild<QMenu*>(QStringLiteral("menu_file")))
  {
    file_menu->addSeparator();
    file_menu->addMenu(view_menu);
  }
}

void MainWindow::update_property_panel(entt::entity entity)
{
  if (!m_propertyPanel)
  {
    return;
  }
  if (m_documentScope)
  {
    m_propertyPanel->SetAdapter(&m_documentScope->scene());
  }
  m_propertyPanel->ShowEntity(m_world.registry(), entity);
}

}  // namespace brep::viewer
