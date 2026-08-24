#include "MainWindow.h"

#include "assets/AssetCatalog.h"
#include "commands/snap/Accusnap.h"
#include "i18n/LanguageManager.h"
#include "ui/SnapSettingsDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QDialog>
#include <QDockWidget>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSize>
#include <QToolBar>

namespace brep::viewer
{

void MainWindow::refresh_edit_actions()
{
  if (m_actUndo)
{
    m_actUndo->setEnabled(m_commandManager.history().can_undo());
    const QString label = m_commandManager.history().undo_label();
    m_actUndo->setText(label.isEmpty() ? tr("&Undo")
                                       : tr("&Undo %1").arg(label));
  }
  if (m_actRedo)
  {
    m_actRedo->setEnabled(m_commandManager.history().can_redo());
    const QString label = m_commandManager.history().redo_label();
    m_actRedo->setText(label.isEmpty() ? tr("&Redo")
                                       : tr("&Redo %1").arg(label));
  }
}

void MainWindow::setup_window_menu()
{
  auto* window_menu = menuBar()->addMenu(tr("&Window"));
  window_menu->setObjectName(QStringLiteral("menu_window"));

  auto* act_new = window_menu->addAction(tr("&New View"));
  act_new->setObjectName(QStringLiteral("act_window_new_view"));
  act_new->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
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
          [](QAction* action)
  {
            if (!action) return;
            LanguageManager::instance().set_preference(
                action->data().toString());
          });
  sync_language_menu_checks();
}

void MainWindow::sync_language_menu_checks()
{
  const QString pref = LanguageManager::instance().preference();
  if (m_actLangSystem) m_actLangSystem->setChecked(pref == QLatin1String("system"));
  if (m_actLangZh) m_actLangZh->setChecked(pref == QLatin1String("zh_CN"));
  if (m_actLangEn) m_actLangEn->setChecked(pref == QLatin1String("en"));
}

void MainWindow::setup_menus()
{
  auto* file_menu = menuBar()->addMenu(tr("&File"));
  file_menu->setObjectName(QStringLiteral("menu_file"));

  auto* act_new = file_menu->addAction(tr("&New"));
  act_new->setObjectName(QStringLiteral("act_file_new"));
  act_new->setShortcut(QKeySequence::New);
  bind_action(act_new, "doc.new");

  auto* act_open = file_menu->addAction(tr("&Open…"));
  act_open->setObjectName(QStringLiteral("act_file_open"));
  act_open->setShortcut(QKeySequence::Open);
  bind_action(act_open, "file.open");

  auto* act_save = file_menu->addAction(tr("&Save"));
  act_save->setObjectName(QStringLiteral("act_file_save"));
  act_save->setShortcut(QKeySequence::Save);
  bind_action(act_save, "file.save");

  file_menu->addSeparator();

  auto* act_export = file_menu->addAction(tr("&Export DWG/DXF…"));
  act_export->setObjectName(QStringLiteral("act_file_export"));
  act_export->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
  bind_action(act_export, "file.export_dxf");

  auto* edit_menu = menuBar()->addMenu(tr("&Edit"));
  edit_menu->setObjectName(QStringLiteral("menu_edit"));
  m_actUndo = edit_menu->addAction(tr("&Undo"));
  m_actUndo->setObjectName(QStringLiteral("act_edit_undo"));
  m_actUndo->setShortcut(QKeySequence::Undo);
  bind_action(m_actUndo, "edit.undo");

  m_actRedo = edit_menu->addAction(tr("&Redo"));
  m_actRedo->setObjectName(QStringLiteral("act_edit_redo"));
  m_actRedo->setShortcut(QKeySequence::Redo);
  bind_action(m_actRedo, "edit.redo");

  edit_menu->addSeparator();
  auto* act_copy = edit_menu->addAction(tr("&Copy…"));
  act_copy->setObjectName(QStringLiteral("act_edit_copy"));
  act_copy->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
  act_copy->setToolTip(
      tr("Copy selected boxes: base point → place point"));
  bind_action(act_copy, "edit.copy");

  auto* act_delete = edit_menu->addAction(tr("&Delete"));
  act_delete->setObjectName(QStringLiteral("act_edit_delete"));
  act_delete->setShortcut(QKeySequence::Delete);
  act_delete->setToolTip(tr("Delete selected objects"));
  bind_action(act_delete, "edit.delete");

  auto* model_menu = menuBar()->addMenu(tr("&Modeling"));
  model_menu->setObjectName(QStringLiteral("menu_model"));
  auto* act_box = model_menu->addAction(tr("Create &Box…"));
  act_box->setObjectName(QStringLiteral("act_model_box"));
  act_box->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
  act_box->setToolTip(tr("Three-point box: base corners + height"));
  bind_action(act_box, "part.create_box");

  auto* act_sphere = model_menu->addAction(tr("Create &Sphere…"));
  act_sphere->setObjectName(QStringLiteral("act_model_sphere"));
  act_sphere->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
  act_sphere->setToolTip(
      tr("Two-point sphere: center on surface/ground + radius"));
  bind_action(act_sphere, "part.create_sphere");

  auto* act_box_fast = model_menu->addAction(tr("Quick Box (default size)"));
  act_box_fast->setObjectName(QStringLiteral("act_model_box_fast"));
  act_box_fast->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")));
  bind_action(act_box_fast, "part.create_box_instant");

  model_menu->addSeparator();
  const QString bool_tip = tr(
      "Requires exactly 2 selected bodies. Subtract: primary selection = "
      "target, secondary = tool.");
  auto* act_bool_union = model_menu->addAction(tr("Boolean &Union (Fuse)"));
  act_bool_union->setObjectName(QStringLiteral("act_model_boolean_union"));
  act_bool_union->setToolTip(bool_tip);
  bind_action(act_bool_union, "boolean.union");

  auto* act_bool_sub = model_menu->addAction(tr("Boolean &Subtract (Cut)"));
  act_bool_sub->setObjectName(QStringLiteral("act_model_boolean_subtract"));
  act_bool_sub->setToolTip(bool_tip);
  bind_action(act_bool_sub, "boolean.subtract");

  auto* act_bool_int = model_menu->addAction(tr("Boolean &Intersect (Common)"));
  act_bool_int->setObjectName(QStringLiteral("act_model_boolean_intersect"));
  act_bool_int->setToolTip(bool_tip);
  bind_action(act_bool_int, "boolean.intersect");

  auto* tools_menu = menuBar()->addMenu(tr("&Tools"));
  tools_menu->setObjectName(QStringLiteral("menu_tools"));
  auto* act_palette = tools_menu->addAction(tr("Command &Palette…"));
  act_palette->setObjectName(QStringLiteral("act_tools_palette"));
  act_palette->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
  connect(act_palette, &QAction::triggered, this,
          &MainWindow::on_command_palette);

  tools_menu->addSeparator();
  auto* act_snap_settings =
      tools_menu->addAction(tr("Snap &Settings…"));
  act_snap_settings->setObjectName(
      QStringLiteral("act_tools_snap_settings"));
  connect(act_snap_settings, &QAction::triggered, this,
          &MainWindow::show_snap_settings);

  tools_menu->addSeparator();
  setup_language_menu(tools_menu);

  setup_window_menu();
}

void MainWindow::setup_toolbar()
{
  m_toolbar = addToolBar(tr("Main Toolbar"));
  m_toolbar->setObjectName(QStringLiteral("MainToolbar"));
  m_toolbar->setMovable(false);
  m_toolbar->setIconSize(QSize(20, 20));

  auto* act_new = m_toolbar->addAction(tr("New"));
  act_new->setObjectName(QStringLiteral("tb_new"));
  bind_action(act_new, "doc.new");

  auto* act_open = m_toolbar->addAction(tr("Open"));
  act_open->setObjectName(QStringLiteral("tb_open"));
  bind_action(act_open, "file.open");

  auto* act_save = m_toolbar->addAction(tr("Save"));
  act_save->setObjectName(QStringLiteral("tb_save"));
  bind_action(act_save, "file.save");

  auto* act_box = m_toolbar->addAction(tr("Box"));
  act_box->setObjectName(QStringLiteral("tb_box"));
  act_box->setToolTip(tr("Three-point box (Ctrl+B)"));
  bind_action(act_box, "part.create_box");

  auto* act_sphere = m_toolbar->addAction(tr("Sphere"));
  act_sphere->setObjectName(QStringLiteral("tb_sphere"));
  act_sphere->setToolTip(tr("Two-point sphere (Ctrl+Shift+S)"));
  bind_action(act_sphere, "part.create_sphere");

  m_toolbar->addSeparator();
  const QString bool_tip = tr(
      "Requires exactly 2 selected bodies. Subtract: primary selection = "
      "target, secondary = tool.");
  auto* act_fuse = m_toolbar->addAction(tr("Fuse"));
  act_fuse->setObjectName(QStringLiteral("tb_boolean_union"));
  act_fuse->setToolTip(bool_tip);
  bind_action(act_fuse, "boolean.union");

  auto* act_cut = m_toolbar->addAction(tr("Cut"));
  act_cut->setObjectName(QStringLiteral("tb_boolean_subtract"));
  act_cut->setToolTip(bool_tip);
  bind_action(act_cut, "boolean.subtract");

  auto* act_common = m_toolbar->addAction(tr("Common"));
  act_common->setObjectName(QStringLiteral("tb_boolean_intersect"));
  act_common->setToolTip(bool_tip);
  bind_action(act_common, "boolean.intersect");

  auto* act_copy = m_toolbar->addAction(tr("Copy"));
  act_copy->setObjectName(QStringLiteral("tb_copy"));
  act_copy->setToolTip(tr("Copy selected objects (Ctrl+Shift+C)"));
  bind_action(act_copy, "edit.copy");

  auto* act_export = m_toolbar->addAction(tr("Export DXF"));
  act_export->setObjectName(QStringLiteral("tb_export"));
  bind_action(act_export, "file.export_dxf");

  m_toolbar->addSeparator();
  m_actSnapEnabled = m_toolbar->addAction(tr("Snap"));
  m_actSnapEnabled->setObjectName(QStringLiteral("tb_snap_enabled"));
  m_actSnapEnabled->setCheckable(true);
  m_actSnapEnabled->setChecked(m_snapSettings.enabled);
  m_actSnapEnabled->setToolTip(tr("Enable AccuSnap (F3)"));
  connect(m_actSnapEnabled, &QAction::toggled, this,
          &MainWindow::set_snap_enabled);
}

void MainWindow::show_snap_settings()
{
  SnapSettingsDialog dialog(m_snapSettings, this);
  if (dialog.exec() != QDialog::Accepted) return;
  m_snapSettings = dialog.snap_settings();
  save_snap_settings();
  sync_snap_action();
  auto ctx = make_command_context();
  commands::AccuSnap::clear_feedback(ctx);
  request_all_views_update();
}

void MainWindow::set_snap_enabled(bool enabled)
{
  m_snapSettings.enabled = enabled;
  save_snap_settings();
  sync_snap_action();
  auto ctx = make_command_context();
  commands::AccuSnap::clear_feedback(ctx);
  request_all_views_update();
}

void MainWindow::save_snap_settings()
{
  QSettings storage;
  commands::save_snap_settings(storage, m_snapSettings);
}

void MainWindow::sync_snap_action()
{
  if (!m_actSnapEnabled) return;
  const QSignalBlocker blocker(m_actSnapEnabled);
  m_actSnapEnabled->setChecked(m_snapSettings.enabled);
}

void MainWindow::setup_view_toolbar()
{
  m_viewToolbar = addToolBar(tr("View Orientation"));
  m_viewToolbar->setObjectName(QStringLiteral("ViewOrientToolbar"));
  m_viewToolbar->setMovable(true);
  m_viewToolbar->setIconSize(QSize(32, 32));
  m_viewToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  struct Spec
{
    const char* file;
    const char* object_name;
    char face;
  };
  constexpr Spec kSpecs[] = {
      {"front.png", "tb_view_front", 'f'},
      {"back.png", "tb_view_back", 'k'},
      {"left.png", "tb_view_left", 'l'},
      {"right.png", "tb_view_right", 'r'},
      {"top.png", "tb_view_top", 't'},
      {"bottom.png", "tb_view_bottom", 'b'},
      {"iso.png", "tb_view_iso", 'h'},
  };

  for (const auto& spec : kSpecs)
  {
    const QIcon icon = AssetCatalog::icon(QStringLiteral("views/") +
                                          QString::fromUtf8(spec.file));
    auto* act = m_viewToolbar->addAction(icon, QString());
    act->setObjectName(QString::fromUtf8(spec.object_name));
    const char face = spec.face;
    connect(act, &QAction::triggered, this,
            [this, face] { apply_standard_view(face); });
  }

  m_viewToolbar->addSeparator();
  m_actOrtho = m_viewToolbar->addAction(tr("Ortho"));
  m_actOrtho->setObjectName(QStringLiteral("tb_view_ortho"));
  m_actOrtho->setCheckable(true);
  m_actOrtho->setToolTip(tr("Orthographic projection"));
  if (auto* vw = active_vulkan_window())
  {
    m_actOrtho->setChecked(vw->camera().ortho);
  }
  connect(m_actOrtho, &QAction::toggled, this, [this](bool on)
  {
    if (auto* vw = active_vulkan_window())
  {
      vw->camera().ortho = on;
      if (m_viewCube) m_viewCube->update();
      vw->requestUpdate();
    }
  });
}

void MainWindow::retranslate_ui()
{
  auto set_menu = [this](const char* name, const QString& title)
{
    if (auto* m = findChild<QMenu*>(QLatin1String(name))) m->setTitle(title);
  };
  auto set_act = [this](const char* name, const QString& text)
  {
    if (auto* a = findChild<QAction*>(QLatin1String(name))) a->setText(text);
  };
  auto set_tip = [this](const char* name, const QString& tip)
  {
    if (auto* a = findChild<QAction*>(QLatin1String(name))) a->setToolTip(tip);
  };

  set_menu("menu_file", tr("&File"));
  set_act("act_file_new", tr("&New"));
  set_act("act_file_open", tr("&Open…"));
  set_act("act_file_save", tr("&Save"));
  set_act("act_file_export", tr("&Export DWG/DXF…"));

  set_menu("menu_edit", tr("&Edit"));
  set_act("act_edit_copy", tr("&Copy…"));
  set_tip("act_edit_copy",
          tr("Copy selected boxes: base point → place point"));
  set_act("act_edit_delete", tr("&Delete"));
  set_tip("act_edit_delete", tr("Delete selected objects"));

  set_menu("menu_model", tr("&Modeling"));
  set_act("act_model_box", tr("Create &Box…"));
  set_tip("act_model_box", tr("Three-point box: base corners + height"));
  set_act("act_model_sphere", tr("Create &Sphere…"));
  set_tip("act_model_sphere",
          tr("Two-point sphere: center on surface/ground + radius"));
  set_act("act_model_box_fast", tr("Quick Box (default size)"));
  set_act("act_model_boolean_union", tr("Boolean &Union (Fuse)"));
  set_act("act_model_boolean_subtract", tr("Boolean &Subtract (Cut)"));
  set_act("act_model_boolean_intersect", tr("Boolean &Intersect (Common)"));
  const QString bool_tip = tr(
      "Requires exactly 2 selected bodies. Subtract: primary selection = "
      "target, secondary = tool.");
  set_tip("act_model_boolean_union", bool_tip);
  set_tip("act_model_boolean_subtract", bool_tip);
  set_tip("act_model_boolean_intersect", bool_tip);

  set_menu("menu_tools", tr("&Tools"));
  set_act("act_tools_palette", tr("Command &Palette…"));
  set_act("act_tools_snap_settings", tr("Snap &Settings…"));
  set_menu("menu_language", tr("&Language"));
  set_act("act_lang_system", tr("Follow System"));
  set_act("act_lang_zh", tr("Simplified Chinese"));
  set_act("act_lang_en", tr("English"));

  set_menu("menu_window", tr("&Window"));
  set_act("act_window_new_view", tr("&New View"));
  set_act("act_window_quad", tr("&Quad Views"));
  set_act("act_window_tile", tr("&Tile"));
  set_act("act_window_cascade", tr("&Cascade"));
  set_act("act_window_close", tr("C&lose Active View"));

  set_menu("menu_view", tr("&View"));

  if (m_toolbar) m_toolbar->setWindowTitle(tr("Main Toolbar"));
  set_act("tb_new", tr("New"));
  set_act("tb_open", tr("Open"));
  set_act("tb_save", tr("Save"));
  set_act("tb_box", tr("Box"));
  set_tip("tb_box", tr("Three-point box (Ctrl+B)"));
  set_act("tb_sphere", tr("Sphere"));
  set_tip("tb_sphere", tr("Two-point sphere (Ctrl+Shift+S)"));
  set_act("tb_boolean_union", tr("Fuse"));
  set_act("tb_boolean_subtract", tr("Cut"));
  set_act("tb_boolean_intersect", tr("Common"));
  const QString tb_bool_tip = tr(
      "Requires exactly 2 selected bodies. Subtract: primary selection = "
      "target, secondary = tool.");
  set_tip("tb_boolean_union", tb_bool_tip);
  set_tip("tb_boolean_subtract", tb_bool_tip);
  set_tip("tb_boolean_intersect", tb_bool_tip);
  set_act("tb_copy", tr("Copy"));
  set_tip("tb_copy", tr("Copy selected objects (Ctrl+Shift+C)"));
  set_act("tb_export", tr("Export DXF"));
  set_act("tb_snap_enabled", tr("Snap"));
  set_tip("tb_snap_enabled", tr("Enable AccuSnap (F3)"));

  if (m_viewToolbar) m_viewToolbar->setWindowTitle(tr("View Orientation"));
  set_act("tb_view_front", tr("Front"));
  set_tip("tb_view_front", tr("Front"));
  set_act("tb_view_back", tr("Back"));
  set_tip("tb_view_back", tr("Back"));
  set_act("tb_view_left", tr("Left"));
  set_tip("tb_view_left", tr("Left"));
  set_act("tb_view_right", tr("Right"));
  set_tip("tb_view_right", tr("Right"));
  set_act("tb_view_top", tr("Top"));
  set_tip("tb_view_top", tr("Top"));
  set_act("tb_view_bottom", tr("Bottom"));
  set_tip("tb_view_bottom", tr("Bottom"));
  set_act("tb_view_iso", tr("Isometric"));
  set_tip("tb_view_iso", tr("Isometric"));
  set_act("tb_view_ortho", tr("Ortho"));
  set_tip("tb_view_ortho", tr("Orthographic projection"));

  if (m_propertyDock) m_propertyDock->setWindowTitle(tr("Properties"));
  if (m_propertyPanel) m_propertyPanel->retranslate_ui();

  refresh_view_titles();
  refresh_edit_actions();
  sync_language_menu_checks();
  refresh_cursor_tip();
}

}  // namespace brep::viewer
