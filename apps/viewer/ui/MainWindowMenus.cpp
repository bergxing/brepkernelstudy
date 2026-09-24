#include "MainWindow.h"

#include "assets/AssetCatalog.h"
#include "commands/snap/Accusnap.h"
#include "ecs/Components.h"
#include "i18n/LanguageManager.h"
#include "ui/RibbonSetup.h"
#include "ui/SnapSettingsDialog.h"
#include "ui/ThemeSettingsDialog.h"

#include "SARibbonActionsManager.h"
#include "SARibbonApplicationButton.h"
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonCustomizeDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QAbstractButton>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>

namespace brep::viewer
{

void MainWindow::refresh_edit_actions()
{
  if (commands::ITool* tool = m_commandManager.active_tool();
      tool && tool->OwnsUndoRedo())
  {
    if (m_actUndo)
    {
      m_actUndo->setEnabled(tool->CanUndoStep());
      m_actUndo->setText(tr("&Undo"));
      m_actUndo->setToolTip(tr("Undo last point"));
    }
    if (m_actRedo)
    {
      m_actRedo->setEnabled(tool->CanRedoStep());
      m_actRedo->setText(tr("&Redo"));
      m_actRedo->setToolTip(tr("Redo last point"));
    }
    return;
  }
  if (m_actUndo)
  {
    m_actUndo->setEnabled(m_commandManager.history().can_undo());
    m_actUndo->setText(tr("&Undo"));
    const QString label = m_commandManager.history().undo_label();
    m_actUndo->setToolTip(label.isEmpty() ? tr("Undo")
                                          : tr("Undo %1").arg(label));
  }
  if (m_actRedo)
  {
    m_actRedo->setEnabled(m_commandManager.history().can_redo());
    m_actRedo->setText(tr("&Redo"));
    const QString label = m_commandManager.history().redo_label();
    m_actRedo->setToolTip(label.isEmpty() ? tr("Redo")
                                          : tr("Redo %1").arg(label));
  }
}

void MainWindow::setup_action_catalog()
{
  m_actions = new ActionCatalog(this);
  m_actUndo = m_actions->Undo();
  m_actRedo = m_actions->Redo();
  m_actSnapEnabled = m_actions->SnapToggle();
  m_actOrtho = m_actions->Ortho();
  if (m_actSnapEnabled)
  {
    m_actSnapEnabled->setChecked(m_snapSettings.enabled);
  }
  if (m_actOrtho)
  {
    m_actOrtho->setChecked(true);
  }

  connect(m_actions, &ActionCatalog::CommandTriggered, this,
          [this](const QString& id) { run_command(id.toStdString()); });
  connect(m_actions, &ActionCatalog::PaletteRequested, this,
          &MainWindow::on_command_palette);
  connect(m_actions, &ActionCatalog::SnapSettingsRequested, this,
          &MainWindow::show_snap_settings);
  connect(m_actions, &ActionCatalog::ThemeSettingsRequested, this,
          &MainWindow::show_theme_settings);
  connect(m_actions, &ActionCatalog::CustomizeRibbonRequested, this,
          &MainWindow::on_customize_ribbon);
  connect(m_actions, &ActionCatalog::ResetRibbonRequested, this,
          &MainWindow::on_reset_ribbon);
  connect(m_actions, &ActionCatalog::SnapToggled, this,
          &MainWindow::set_snap_enabled);
  connect(m_actions, &ActionCatalog::OrthoToggled, this, [this](bool on) {
    if (auto* vw = active_vulkan_window())
    {
      vw->camera().ortho = on;
      if (m_viewCube)
      {
        m_viewCube->update();
      }
      vw->requestUpdate();
    }
  });
  connect(m_actions, &ActionCatalog::StandardViewRequested, this,
          &MainWindow::apply_standard_view);
}

void MainWindow::setup_window_menu()
{
  // SARibbonBar is itself a QMenuBar — do not menuBar()->addMenu or menus
  // appear as hover popups next to ribbon tabs. Keep a private QMenu for
  // Window actions (shortcuts still work via QAction).
  auto* window_menu = new QMenu(tr("&Window"), this);
  window_menu->setObjectName(QStringLiteral("menu_window"));

  auto* act_new = window_menu->addAction(tr("&New View"));
  act_new->setObjectName(QStringLiteral("act_window_new_view"));
  act_new->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
  act_new->setShortcutContext(Qt::WindowShortcut);
  addAction(act_new);
  connect(act_new, &QAction::triggered, this, &MainWindow::on_new_view);

  auto* act_quad = window_menu->addAction(tr("&Quad Views"));
  act_quad->setObjectName(QStringLiteral("act_window_quad"));
  connect(act_quad, &QAction::triggered, this, &MainWindow::on_quad_views);

  window_menu->addSeparator();

  auto* act_tile = window_menu->addAction(tr("&Tile"));
  act_tile->setObjectName(QStringLiteral("act_window_tile"));
  connect(act_tile, &QAction::triggered, this, &MainWindow::on_tile_views);

  auto* act_cascade = window_menu->addAction(tr("&Cascade"));
  act_cascade->setObjectName(QStringLiteral("act_window_cascade"));
  connect(act_cascade, &QAction::triggered, this, &MainWindow::on_cascade_views);

  window_menu->addSeparator();

  auto* act_close = window_menu->addAction(tr("C&lose Active View"));
  act_close->setObjectName(QStringLiteral("act_window_close"));
  connect(act_close, &QAction::triggered, this,
          &MainWindow::on_close_active_view);
}

void MainWindow::setup_language_menu(QMenu* tools_menu)
{
  auto* lang_menu = tools_menu->addMenu(tr("&Language"));
  lang_menu->setObjectName(QStringLiteral("menu_language"));

  m_langActionGroup = new QActionGroup(this);
  m_langActionGroup->setExclusive(true);

  m_actLangSystem = m_langActionGroup->addAction(tr("Follow System"));
  m_actLangSystem->setObjectName(QStringLiteral("act_lang_system"));
  m_actLangSystem->setCheckable(true);
  m_actLangSystem->setData(QStringLiteral("system"));
  lang_menu->addAction(m_actLangSystem);

  m_actLangZh = m_langActionGroup->addAction(tr("Simplified Chinese"));
  m_actLangZh->setObjectName(QStringLiteral("act_lang_zh"));
  m_actLangZh->setCheckable(true);
  m_actLangZh->setData(QStringLiteral("zh_CN"));
  lang_menu->addAction(m_actLangZh);

  m_actLangEn = m_langActionGroup->addAction(tr("English"));
  m_actLangEn->setObjectName(QStringLiteral("act_lang_en"));
  m_actLangEn->setCheckable(true);
  m_actLangEn->setData(QStringLiteral("en"));
  lang_menu->addAction(m_actLangEn);

  connect(m_langActionGroup, &QActionGroup::triggered, this,
          [](QAction* action) {
            if (!action)
            {
              return;
            }
            LanguageManager::instance().set_preference(
                action->data().toString());
          });
  sync_language_menu_checks();
}

void MainWindow::sync_language_menu_checks()
{
  const QString pref = LanguageManager::instance().preference();
  if (m_actLangSystem)
  {
    m_actLangSystem->setChecked(pref == QLatin1String("system"));
  }
  if (m_actLangZh)
  {
    m_actLangZh->setChecked(pref == QLatin1String("zh_CN"));
  }
  if (m_actLangEn)
  {
    m_actLangEn->setChecked(pref == QLatin1String("en"));
  }
}

void MainWindow::setup_menus()
{
  if (!m_actions)
  {
    return;
  }

  // File menu lives on the Ribbon application button (Office-style).
  auto* file_menu = new QMenu(tr("&File"), this);
  file_menu->setObjectName(QStringLiteral("menu_file"));
  file_menu->addAction(m_actions->Action(QStringLiteral("doc.new")));
  file_menu->addAction(m_actions->Action(QStringLiteral("file.open")));
  file_menu->addAction(m_actions->Action(QStringLiteral("file.save")));
  file_menu->addSeparator();
  file_menu->addAction(m_actions->Action(QStringLiteral("file.export_dxf")));
  file_menu->addSeparator();

  auto* edit_menu = file_menu->addMenu(tr("&Edit"));
  edit_menu->setObjectName(QStringLiteral("menu_edit"));
  edit_menu->addAction(m_actUndo);
  edit_menu->addAction(m_actRedo);
  edit_menu->addSeparator();
  edit_menu->addAction(m_actions->Action(QStringLiteral("edit.copy")));
  edit_menu->addAction(m_actions->Action(QStringLiteral("edit.move")));
  edit_menu->addAction(m_actions->Action(QStringLiteral("edit.delete")));

  auto* model_menu = file_menu->addMenu(tr("&Modeling"));
  model_menu->setObjectName(QStringLiteral("menu_model"));
  model_menu->addAction(m_actions->Action(QStringLiteral("part.create_box")));
  model_menu->addAction(m_actions->Action(QStringLiteral("part.extrude_pad")));
  model_menu->addAction(m_actions->Action(QStringLiteral("part.create_sphere")));
  model_menu->addAction(m_actions->Action(QStringLiteral("part.create_bezier")));
  model_menu->addAction(
      m_actions->Action(QStringLiteral("part.create_nurbs_curve")));
  model_menu->addAction(
      m_actions->Action(QStringLiteral("part.create_box_instant")));
  model_menu->addSeparator();
  model_menu->addAction(m_actions->Action(QStringLiteral("boolean.union")));
  model_menu->addAction(m_actions->Action(QStringLiteral("boolean.subtract")));
  model_menu->addAction(m_actions->Action(QStringLiteral("boolean.intersect")));

  if (SARibbonBar* bar = ribbonBar())
  {
    QAbstractButton* appBtn = bar->applicationButton();
    if (!appBtn)
    {
      appBtn = new SARibbonApplicationButton(this);
      bar->setApplicationButton(appBtn);
    }
    appBtn->setText(tr("&File"));
    if (auto* ribbonApp = qobject_cast<SARibbonApplicationButton*>(appBtn))
    {
      ribbonApp->setMenu(file_menu);
    }
  }

  auto* tools_menu = new QMenu(tr("&Tools"), this);
  tools_menu->setObjectName(QStringLiteral("menu_tools"));
  tools_menu->addAction(m_actions->Action(QStringLiteral("tools.palette")));
  tools_menu->addSeparator();
  tools_menu->addAction(
      m_actions->Action(QStringLiteral("tools.snap_settings")));
  tools_menu->addAction(m_actions->Action(QStringLiteral("tools.theme")));
  tools_menu->addSeparator();
  tools_menu->addAction(m_actions->CustomizeRibbon());
  tools_menu->addAction(m_actions->ResetRibbon());
  tools_menu->addSeparator();
  setup_language_menu(tools_menu);

  setup_window_menu();

  // Application button: File + Tools/Window (menus must not go on SARibbonBar).
  file_menu->addSeparator();
  file_menu->addMenu(tools_menu);
  if (auto* window_menu = findChild<QMenu*>(QStringLiteral("menu_window")))
  {
    file_menu->addMenu(window_menu);
  }
}

void MainWindow::rebuild_ribbon_default()
{
  SARibbonBar* bar = ribbonBar();
  if (!bar || !m_actions)
  {
    return;
  }
  const QList<SARibbonCategory*> pages = bar->categoryPages(true);
  for (SARibbonCategory* cat : pages)
  {
    bar->removeCategory(cat);
  }
  delete m_ribbonActions;
  m_ribbonActions = new SARibbonActionsManager(bar);
  RibbonSetup::BuildDefault(bar, *m_actions, m_ribbonActions);
}

void MainWindow::setup_ribbon()
{
  rebuild_ribbon_default();
  RibbonSetup::LoadLayout(ribbonBar(), m_ribbonActions, RibbonSetup::LayoutPath());
  RibbonSetup::SetupQuickAccessBar(ribbonBar(), *m_actions);
  RibbonSetup::SetupRibbonChrome(ribbonBar(), *m_actions);
}

void MainWindow::on_customize_ribbon()
{
  if (!ribbonBar() || !m_ribbonActions)
  {
    return;
  }
  ::SARibbonCustomizeDialog dialog(this);
  dialog.setupActionsManager(m_ribbonActions);
  dialog.fromXml(RibbonSetup::LayoutPath());
  if (dialog.exec() != QDialog::Accepted)
  {
    return;
  }
  if (dialog.isCached())
  {
    dialog.applys();
  }
  if (dialog.isApplied())
  {
    dialog.toXml(RibbonSetup::LayoutPath());
  }
}

void MainWindow::on_reset_ribbon()
{
  const auto answer = QMessageBox::question(
      this, tr("Reset Ribbon"),
      tr("Reset the ribbon layout to defaults?"));
  if (answer != QMessageBox::Yes)
  {
    return;
  }
  QFile::remove(RibbonSetup::LayoutPath());
  rebuild_ribbon_default();
  RibbonSetup::SetupQuickAccessBar(ribbonBar(), *m_actions);
  RibbonSetup::SetupRibbonChrome(ribbonBar(), *m_actions);
  statusBar()->showMessage(tr("Ribbon layout reset"), 3000);
}

void MainWindow::show_snap_settings()
{
  SnapSettingsDialog dialog(m_snapSettings, this);
  if (dialog.exec() != QDialog::Accepted)
  {
    return;
  }
  m_snapSettings = dialog.snap_settings();
  save_snap_settings();
  sync_snap_action();
  auto ctx = make_command_context();
  commands::AccuSnap::ClearFeedback(ctx);
  request_all_views_update();
}

void MainWindow::show_theme_settings()
{
  ThemeSettingsDialog dialog(m_viewerTheme, this);
  if (dialog.exec() != QDialog::Accepted)
  {
    return;
  }
  m_viewerTheme = dialog.Theme();
  QSettings storage;
  SaveViewerTheme(storage, m_viewerTheme);
  apply_viewer_theme();
}

void MainWindow::apply_viewer_theme()
{
  ApplyQtTheme(*qApp, m_viewerTheme.Mode);
  RibbonSetup::ApplyTheme(this, m_actions,
                          m_viewerTheme.Mode == ThemeMode::Light);
  float clearRgb[3];
  float wireRgb[3];
  float hoverRgb[3];
  float previewRgb[3];
  m_viewerTheme.Resolve(clearRgb, wireRgb, hoverRgb, previewRgb);
  for (auto* window : m_viewWindows)
  {
    if (!window)
    {
      continue;
    }
    window->set_viewport_colors(clearRgb[0], clearRgb[1], clearRgb[2],
                                wireRgb[0], wireRgb[1], wireRgb[2],
                                hoverRgb[0], hoverRgb[1], hoverRgb[2],
                                previewRgb[0], previewRgb[1], previewRgb[2]);
  }
  if (m_world.registry().ctx().contains<ecs::RenderCache>())
  {
    m_world.registry().ctx().get<ecs::RenderCache>().force_rebuild = true;
  }
  request_all_views_update();
}

void MainWindow::set_snap_enabled(bool enabled)
{
  m_snapSettings.enabled = enabled;
  save_snap_settings();
  sync_snap_action();
  auto ctx = make_command_context();
  commands::AccuSnap::ClearFeedback(ctx);
  request_all_views_update();
}

void MainWindow::save_snap_settings()
{
  QSettings storage;
  commands::save_snap_settings(storage, m_snapSettings);
}

void MainWindow::sync_snap_action()
{
  if (!m_actSnapEnabled)
  {
    return;
  }
  const QSignalBlocker blocker(m_actSnapEnabled);
  m_actSnapEnabled->setChecked(m_snapSettings.enabled);
}

void MainWindow::retranslate_ui()
{
  auto set_menu = [this](const char* name, const QString& title) {
    if (auto* m = findChild<QMenu*>(QLatin1String(name)))
    {
      m->setTitle(title);
    }
  };

  set_menu("menu_file", tr("&File"));
  set_menu("menu_edit", tr("&Edit"));
  set_menu("menu_model", tr("&Modeling"));
  set_menu("menu_tools", tr("&Tools"));
  set_menu("menu_language", tr("&Language"));
  set_menu("menu_window", tr("&Window"));
  set_menu("menu_view", tr("&View"));

  if (auto* a = findChild<QAction*>(QStringLiteral("act_lang_system")))
  {
    a->setText(tr("Follow System"));
  }
  if (auto* a = findChild<QAction*>(QStringLiteral("act_lang_zh")))
  {
    a->setText(tr("Simplified Chinese"));
  }
  if (auto* a = findChild<QAction*>(QStringLiteral("act_lang_en")))
  {
    a->setText(tr("English"));
  }
  if (auto* a = findChild<QAction*>(QStringLiteral("act_window_new_view")))
  {
    a->setText(tr("&New View"));
  }
  if (auto* a = findChild<QAction*>(QStringLiteral("act_window_quad")))
  {
    a->setText(tr("&Quad Views"));
  }
  if (auto* a = findChild<QAction*>(QStringLiteral("act_window_tile")))
  {
    a->setText(tr("&Tile"));
  }
  if (auto* a = findChild<QAction*>(QStringLiteral("act_window_cascade")))
  {
    a->setText(tr("&Cascade"));
  }
  if (auto* a = findChild<QAction*>(QStringLiteral("act_window_close")))
  {
    a->setText(tr("C&lose Active View"));
  }

  if (m_actions)
  {
    m_actions->Retranslate();
  }
  RibbonSetup::RetranslateCategories(ribbonBar(), m_ribbonActions);
  refresh_edit_actions();
  if (m_propertyDock)
  {
    m_propertyDock->setWindowTitle(tr("Properties"));
  }
  if (m_propertyPanel)
  {
    m_propertyPanel->retranslate_ui();
  }
  refresh_view_titles();
  sync_language_menu_checks();
}

}  // namespace brep::viewer
