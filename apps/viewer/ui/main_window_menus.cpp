#include "main_window.hpp"

#include "assets/asset_catalog.hpp"
#include "i18n/language_manager.hpp"

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QSize>
#include <QToolBar>

namespace brep::viewer {

void MainWindow::refresh_edit_actions() {
  if (act_undo_) {
    act_undo_->setEnabled(command_manager_.history().can_undo());
    const QString label = command_manager_.history().undo_label();
    act_undo_->setText(label.isEmpty() ? tr("&Undo")
                                       : tr("&Undo %1").arg(label));
  }
  if (act_redo_) {
    act_redo_->setEnabled(command_manager_.history().can_redo());
    const QString label = command_manager_.history().redo_label();
    act_redo_->setText(label.isEmpty() ? tr("&Redo")
                                       : tr("&Redo %1").arg(label));
  }
}

void MainWindow::setup_window_menu() {
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

void MainWindow::setup_language_menu(QMenu* tools_menu) {
  auto* lang_menu = tools_menu->addMenu(tr("&Language"));
  lang_menu->setObjectName(QStringLiteral("menu_language"));

  lang_action_group_ = new QActionGroup(this);
  lang_action_group_->setExclusive(true);

  act_lang_system_ = lang_action_group_->addAction(tr("Follow System"));
  act_lang_system_->setObjectName(QStringLiteral("act_lang_system"));
  act_lang_system_->setCheckable(true);
  act_lang_system_->setData(QStringLiteral("system"));
  lang_menu->addAction(act_lang_system_);

  act_lang_zh_ = lang_action_group_->addAction(tr("Simplified Chinese"));
  act_lang_zh_->setObjectName(QStringLiteral("act_lang_zh"));
  act_lang_zh_->setCheckable(true);
  act_lang_zh_->setData(QStringLiteral("zh_CN"));
  lang_menu->addAction(act_lang_zh_);

  act_lang_en_ = lang_action_group_->addAction(tr("English"));
  act_lang_en_->setObjectName(QStringLiteral("act_lang_en"));
  act_lang_en_->setCheckable(true);
  act_lang_en_->setData(QStringLiteral("en"));
  lang_menu->addAction(act_lang_en_);

  connect(lang_action_group_, &QActionGroup::triggered, this,
          [](QAction* action) {
            if (!action) return;
            LanguageManager::instance().set_preference(
                action->data().toString());
          });
  sync_language_menu_checks();
}

void MainWindow::sync_language_menu_checks() {
  const QString pref = LanguageManager::instance().preference();
  if (act_lang_system_) act_lang_system_->setChecked(pref == QLatin1String("system"));
  if (act_lang_zh_) act_lang_zh_->setChecked(pref == QLatin1String("zh_CN"));
  if (act_lang_en_) act_lang_en_->setChecked(pref == QLatin1String("en"));
}

void MainWindow::setup_menus() {
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
  act_undo_ = edit_menu->addAction(tr("&Undo"));
  act_undo_->setObjectName(QStringLiteral("act_edit_undo"));
  act_undo_->setShortcut(QKeySequence::Undo);
  bind_action(act_undo_, "edit.undo");

  act_redo_ = edit_menu->addAction(tr("&Redo"));
  act_redo_->setObjectName(QStringLiteral("act_edit_redo"));
  act_redo_->setShortcut(QKeySequence::Redo);
  bind_action(act_redo_, "edit.redo");

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

  auto* tools_menu = menuBar()->addMenu(tr("&Tools"));
  tools_menu->setObjectName(QStringLiteral("menu_tools"));
  auto* act_palette = tools_menu->addAction(tr("Command &Palette…"));
  act_palette->setObjectName(QStringLiteral("act_tools_palette"));
  act_palette->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
  connect(act_palette, &QAction::triggered, this,
          &MainWindow::on_command_palette);

  tools_menu->addSeparator();
  setup_language_menu(tools_menu);

  setup_window_menu();
}

void MainWindow::setup_toolbar() {
  toolbar_ = addToolBar(tr("Main Toolbar"));
  toolbar_->setObjectName(QStringLiteral("MainToolbar"));
  toolbar_->setMovable(false);
  toolbar_->setIconSize(QSize(20, 20));

  auto* act_new = toolbar_->addAction(tr("New"));
  act_new->setObjectName(QStringLiteral("tb_new"));
  bind_action(act_new, "doc.new");

  auto* act_open = toolbar_->addAction(tr("Open"));
  act_open->setObjectName(QStringLiteral("tb_open"));
  bind_action(act_open, "file.open");

  auto* act_save = toolbar_->addAction(tr("Save"));
  act_save->setObjectName(QStringLiteral("tb_save"));
  bind_action(act_save, "file.save");

  auto* act_box = toolbar_->addAction(tr("Box"));
  act_box->setObjectName(QStringLiteral("tb_box"));
  act_box->setToolTip(tr("Three-point box (Ctrl+B)"));
  bind_action(act_box, "part.create_box");

  auto* act_sphere = toolbar_->addAction(tr("Sphere"));
  act_sphere->setObjectName(QStringLiteral("tb_sphere"));
  act_sphere->setToolTip(tr("Two-point sphere (Ctrl+Shift+S)"));
  bind_action(act_sphere, "part.create_sphere");

  auto* act_copy = toolbar_->addAction(tr("Copy"));
  act_copy->setObjectName(QStringLiteral("tb_copy"));
  act_copy->setToolTip(tr("Copy selected objects (Ctrl+Shift+C)"));
  bind_action(act_copy, "edit.copy");

  auto* act_export = toolbar_->addAction(tr("Export DXF"));
  act_export->setObjectName(QStringLiteral("tb_export"));
  bind_action(act_export, "file.export_dxf");
}

void MainWindow::setup_view_toolbar() {
  view_toolbar_ = addToolBar(tr("View Orientation"));
  view_toolbar_->setObjectName(QStringLiteral("ViewOrientToolbar"));
  view_toolbar_->setMovable(true);
  view_toolbar_->setIconSize(QSize(32, 32));
  view_toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);

  struct Spec {
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

  for (const auto& spec : kSpecs) {
    const QIcon icon = AssetCatalog::icon(QStringLiteral("views/") +
                                          QString::fromUtf8(spec.file));
    auto* act = view_toolbar_->addAction(icon, QString());
    act->setObjectName(QString::fromUtf8(spec.object_name));
    const char face = spec.face;
    connect(act, &QAction::triggered, this,
            [this, face] { apply_standard_view(face); });
  }

  view_toolbar_->addSeparator();
  act_ortho_ = view_toolbar_->addAction(tr("Ortho"));
  act_ortho_->setObjectName(QStringLiteral("tb_view_ortho"));
  act_ortho_->setCheckable(true);
  act_ortho_->setToolTip(tr("Orthographic projection"));
  if (auto* vw = active_vulkan_window()) {
    act_ortho_->setChecked(vw->camera().ortho);
  }
  connect(act_ortho_, &QAction::toggled, this, [this](bool on) {
    if (auto* vw = active_vulkan_window()) {
      vw->camera().ortho = on;
      if (view_cube_) view_cube_->update();
      vw->requestUpdate();
    }
  });
}

void MainWindow::retranslate_ui() {
  auto set_menu = [this](const char* name, const QString& title) {
    if (auto* m = findChild<QMenu*>(QLatin1String(name))) m->setTitle(title);
  };
  auto set_act = [this](const char* name, const QString& text) {
    if (auto* a = findChild<QAction*>(QLatin1String(name))) a->setText(text);
  };
  auto set_tip = [this](const char* name, const QString& tip) {
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

  set_menu("menu_tools", tr("&Tools"));
  set_act("act_tools_palette", tr("Command &Palette…"));
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

  if (toolbar_) toolbar_->setWindowTitle(tr("Main Toolbar"));
  set_act("tb_new", tr("New"));
  set_act("tb_open", tr("Open"));
  set_act("tb_save", tr("Save"));
  set_act("tb_box", tr("Box"));
  set_tip("tb_box", tr("Three-point box (Ctrl+B)"));
  set_act("tb_sphere", tr("Sphere"));
  set_tip("tb_sphere", tr("Two-point sphere (Ctrl+Shift+S)"));
  set_act("tb_copy", tr("Copy"));
  set_tip("tb_copy", tr("Copy selected objects (Ctrl+Shift+C)"));
  set_act("tb_export", tr("Export DXF"));

  if (view_toolbar_) view_toolbar_->setWindowTitle(tr("View Orientation"));
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

  if (property_dock_) property_dock_->setWindowTitle(tr("Properties"));
  if (property_panel_) property_panel_->retranslate_ui();

  refresh_view_titles();
  refresh_edit_actions();
  sync_language_menu_checks();
  refresh_cursor_tip();
}

}  // namespace brep::viewer
