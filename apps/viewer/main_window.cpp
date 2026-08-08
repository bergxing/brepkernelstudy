#include "main_window.hpp"

#include "brep/brep.hpp"
#include "brep/log.hpp"
#include "commands/command_palette.hpp"
#include "ecs/systems.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QVersionNumber>
#include <QVulkanInstance>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <stdexcept>

namespace brep::viewer {
namespace {

int wheel_delta_y(const QWheelEvent* event) {
  if (event->angleDelta().y() != 0) return event->angleDelta().y();
  if (event->pixelDelta().y() != 0) return event->pixelDelta().y();
  return 0;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), command_manager_(commands_) {
  resize(1100, 720);

  commands::register_builtin_commands(commands_);

  vulkan_instance_ = std::make_unique<QVulkanInstance>();
  vulkan_instance_->setApiVersion(QVersionNumber(1, 2, 0));
  if (!vulkan_instance_->create()) {
    vulkan_instance_->setApiVersion(QVersionNumber(1, 0, 0));
    if (!vulkan_instance_->create()) {
      throw std::runtime_error("Failed to create QVulkanInstance");
    }
  }

  vulkan_window_ = new VulkanWindow();
  vulkan_window_->setVulkanInstance(vulkan_instance_.get());
  vulkan_window_->setSampleCount(1);
  vulkan_window_->set_world(&world_);
  vulkan_window_->set_selection_callback([this](entt::entity entity) {
    update_property_panel(entity);
    if (entity == entt::null) {
      statusBar()->showMessage(QStringLiteral("已取消选择"), 3000);
      return;
    }
    const std::string label =
        ecs::selection_label(world_.registry(), entity);
    statusBar()->showMessage(
        QStringLiteral("已选中: %1")
            .arg(QString::fromStdString(label)),
        6000);
  });
  vulkan_window_->set_tool_motion_callback([this](float x, float y) {
    if (!command_manager_.has_active_tool()) return;
    auto ctx = make_command_context();
    command_manager_.tool_mouse_move(ctx, x, y);
  });
  // QWindowContainer often delivers presses to the QWindow directly; the
  // widget/qApp filters alone are not enough on Windows.
  vulkan_window_->set_tool_press_callback([this](float x, float y, int button) {
    if (!command_manager_.has_active_tool()) return false;
    auto ctx = make_command_context();
    const bool consumed =
        command_manager_.tool_mouse_press(ctx, x, y, button);
    sync_tool_ui();
    refresh_edit_actions();
    return consumed;
  });

  document_.new_blank_document(world_);
  BREP_INFO("ECS scene ready: blank Document + Part + camera");

  viewport_container_ = QWidget::createWindowContainer(vulkan_window_, this);
  viewport_container_->setFocusPolicy(Qt::StrongFocus);
  viewport_container_->setMouseTracking(true);
  viewport_container_->setAttribute(Qt::WA_Hover, true);
  viewport_container_->installEventFilter(vulkan_window_);
  viewport_container_->installEventFilter(this);
  viewport_container_->setFocus();
  setCentralWidget(viewport_container_);
  setMouseTracking(true);

  view_cube_ = new ViewCubeWidget(this);
  view_cube_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                             Qt::WindowDoesNotAcceptFocus);
  view_cube_->setAttribute(Qt::WA_ShowWithoutActivating);
  rebind_view_cube_camera();
  view_cube_->set_redraw_callback([this] {
    if (vulkan_window_) vulkan_window_->requestUpdate();
    if (viewport_container_) {
      viewport_container_->setFocus(Qt::OtherFocusReason);
    }
  });
  place_view_cube();
  view_cube_->hide();

  setup_menus();
  setup_toolbar();
  setup_view_toolbar();
  setup_property_dock();
  refresh_window_title();
  refresh_edit_actions();
  update_property_panel(entt::null);

  qApp->installEventFilter(this);

  statusBar()->showMessage(QStringLiteral(
      "XCAD | 左键单击选择 / 拖动旋转 | 立方体=三点创建 | ESC 取消工具"));
}

MainWindow::~MainWindow() {
  if (tool_cursor_overridden_) {
    QApplication::restoreOverrideCursor();
    tool_cursor_overridden_ = false;
  }
}

QString MainWindow::wood_albedo_path() const {
  QString wood_path = QStringLiteral(BREP_VIEWER_ASSETS_DIR "/wood.png");
  if (!QFileInfo::exists(wood_path)) {
    wood_path = QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("assets/wood.png"));
  }
  return wood_path;
}

QString MainWindow::view_icon_path(const QString& filename) const {
  QString path =
      QDir(QStringLiteral(BREP_VIEWER_ASSETS_DIR)).filePath(
          QStringLiteral("views/") + filename);
  if (!QFileInfo::exists(path)) {
    path = QDir(QCoreApplication::applicationDirPath())
               .filePath(QStringLiteral("assets/views/") + filename);
  }
  return path;
}

void MainWindow::apply_standard_view(char face) {
  Camera* cam = world_.main_camera();
  if (!cam) return;
  cam->set_standard_view(face);
  if (act_ortho_) {
    const QSignalBlocker block(act_ortho_);
    act_ortho_->setChecked(cam->ortho);
  }
  if (view_cube_) view_cube_->update();
  if (vulkan_window_) vulkan_window_->requestUpdate();
}

commands::CommandContext MainWindow::make_command_context() {
  commands::CommandContext ctx;
  ctx.world = &world_;
  ctx.session = &document_;
  ctx.history = &command_manager_.history();
  ctx.parent_widget = this;
  ctx.wood_albedo_path = wood_albedo_path().toStdString();
  // Prefer QVulkanWindow pixel size — matches QWindow mouse coordinates and
  // the swapchain used for picking rays.
  if (vulkan_window_ && vulkan_window_->width() > 0 &&
      vulkan_window_->height() > 0) {
    ctx.viewport_w = vulkan_window_->width();
    ctx.viewport_h = vulkan_window_->height();
  } else if (viewport_container_) {
    ctx.viewport_w = std::max(1, viewport_container_->width());
    ctx.viewport_h = std::max(1, viewport_container_->height());
  }
  ctx.report_status = [this](const QString& msg) {
    // Keep tool prompts visible until the next status update.
    statusBar()->showMessage(msg);
  };
  ctx.request_redraw = [this] {
    if (vulkan_window_) vulkan_window_->requestUpdate();
  };
  ctx.after_document_reset = [this] {
    rebind_view_cube_camera();
    refresh_window_title();
    update_property_panel(entt::null);
  };
  ctx.refresh_ui = [this] {
    refresh_window_title();
    refresh_edit_actions();
  };
  ctx.set_preview_edges = [this](EdgeMesh edges) {
    if (vulkan_window_) vulkan_window_->set_preview_edges(std::move(edges));
  };
  ctx.clear_preview = [this] {
    if (vulkan_window_) vulkan_window_->clear_preview();
  };
  return ctx;
}

void MainWindow::sync_tool_ui() {
  const bool tool = command_manager_.has_active_tool();
  if (vulkan_window_) {
    vulkan_window_->set_selection_enabled(!tool);
  }

  // Use the application override cursor so VulkanWindow::unsetCursor() (called
  // on mouse-release after orbit/pan) cannot clear the pick crosshair.
  if (tool && !tool_cursor_overridden_) {
    QApplication::setOverrideCursor(Qt::CrossCursor);
    tool_cursor_overridden_ = true;
  } else if (!tool && tool_cursor_overridden_) {
    QApplication::restoreOverrideCursor();
    tool_cursor_overridden_ = false;
  }
}

commands::CommandResult MainWindow::run_command(std::string_view command_id) {
  auto ctx = make_command_context();
  auto result = command_manager_.run(command_id, ctx);
  if (result.status == commands::CommandStatus::Failed &&
      !result.message.isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("命令失败"), result.message);
  }
  sync_tool_ui();
  refresh_edit_actions();
  update_property_panel(ecs::selected_entity(world_.registry()));
  if (result.succeeded() || command_manager_.has_active_tool()) {
    refresh_window_title();
  }
  return result;
}

void MainWindow::bind_action(QAction* action, const char* command_id) {
  action->setData(QString::fromUtf8(command_id));
  connect(action, &QAction::triggered, this, &MainWindow::on_run_command);
}

void MainWindow::on_run_command() {
  auto* action = qobject_cast<QAction*>(sender());
  if (!action) return;
  const QString id = action->data().toString();
  if (id.isEmpty()) return;
  run_command(id.toStdString());
}

void MainWindow::on_command_palette() {
  commands::CommandPalette palette(commands_, this);
  if (palette.exec() != QDialog::Accepted) return;
  const QString id = palette.selected_command_id();
  if (!id.isEmpty()) run_command(id.toStdString());
}

void MainWindow::refresh_edit_actions() {
  if (act_undo_) {
    act_undo_->setEnabled(command_manager_.history().can_undo());
    const QString label = command_manager_.history().undo_label();
    act_undo_->setText(label.isEmpty()
                           ? QStringLiteral("撤销(&U)")
                           : QStringLiteral("撤销(&U) %1").arg(label));
  }
  if (act_redo_) {
    act_redo_->setEnabled(command_manager_.history().can_redo());
    const QString label = command_manager_.history().redo_label();
    act_redo_->setText(label.isEmpty()
                           ? QStringLiteral("重做(&R)")
                           : QStringLiteral("重做(&R) %1").arg(label));
  }
}

void MainWindow::setup_menus() {
  auto* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

  auto* act_new = file_menu->addAction(QStringLiteral("新建(&N)"));
  act_new->setShortcut(QKeySequence::New);
  bind_action(act_new, "doc.new");

  auto* act_open = file_menu->addAction(QStringLiteral("打开(&O)…"));
  act_open->setShortcut(QKeySequence::Open);
  bind_action(act_open, "file.open");

  auto* act_save = file_menu->addAction(QStringLiteral("保存(&S)"));
  act_save->setShortcut(QKeySequence::Save);
  bind_action(act_save, "file.save");

  file_menu->addSeparator();

  auto* act_export =
      file_menu->addAction(QStringLiteral("导出 DWG/DXF(&E)…"));
  act_export->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
  bind_action(act_export, "file.export_dxf");

  auto* edit_menu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
  act_undo_ = edit_menu->addAction(QStringLiteral("撤销(&U)"));
  act_undo_->setShortcut(QKeySequence::Undo);
  bind_action(act_undo_, "edit.undo");

  act_redo_ = edit_menu->addAction(QStringLiteral("重做(&R)"));
  act_redo_->setShortcut(QKeySequence::Redo);
  bind_action(act_redo_, "edit.redo");

  auto* model_menu = menuBar()->addMenu(QStringLiteral("建模(&M)"));
  auto* act_box = model_menu->addAction(QStringLiteral("创建立方体(&B)…"));
  act_box->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
  act_box->setToolTip(QStringLiteral("三点拾取创建：底面两点 + 高度点"));
  bind_action(act_box, "part.create_box");

  auto* act_box_fast =
      model_menu->addAction(QStringLiteral("快速立方体（默认尺寸）"));
  act_box_fast->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+B")));
  bind_action(act_box_fast, "part.create_box_instant");

  auto* tools_menu = menuBar()->addMenu(QStringLiteral("工具(&T)"));
  auto* act_palette = tools_menu->addAction(QStringLiteral("命令面板(&P)…"));
  act_palette->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
  connect(act_palette, &QAction::triggered, this,
          &MainWindow::on_command_palette);
}

void MainWindow::setup_toolbar() {
  toolbar_ = addToolBar(QStringLiteral("主工具栏"));
  toolbar_->setObjectName(QStringLiteral("MainToolbar"));
  toolbar_->setMovable(false);
  toolbar_->setIconSize(QSize(20, 20));

  auto* act_new = toolbar_->addAction(QStringLiteral("新建"));
  bind_action(act_new, "doc.new");

  auto* act_open = toolbar_->addAction(QStringLiteral("打开"));
  bind_action(act_open, "file.open");

  auto* act_save = toolbar_->addAction(QStringLiteral("保存"));
  bind_action(act_save, "file.save");

  auto* act_box = toolbar_->addAction(QStringLiteral("立方体"));
  act_box->setToolTip(QStringLiteral("三点创建盒子 (Ctrl+B)"));
  bind_action(act_box, "part.create_box");

  auto* act_export = toolbar_->addAction(QStringLiteral("导出 DXF"));
  bind_action(act_export, "file.export_dxf");
}

void MainWindow::setup_view_toolbar() {
  view_toolbar_ = addToolBar(QStringLiteral("视图方向"));
  view_toolbar_->setObjectName(QStringLiteral("ViewOrientToolbar"));
  view_toolbar_->setMovable(true);
  view_toolbar_->setIconSize(QSize(32, 32));
  view_toolbar_->setToolButtonStyle(Qt::ToolButtonIconOnly);

  struct Spec {
    const char* file;
    const char* tip;
    char face;
  };
  constexpr Spec kSpecs[] = {
      {"front.png", "前视图", 'f'},
      {"back.png", "后视图", 'k'},
      {"left.png", "左视图", 'l'},
      {"right.png", "右视图", 'r'},
      {"top.png", "顶视图", 't'},
      {"bottom.png", "底视图", 'b'},
      {"iso.png", "轴侧视图", 'h'},
  };

  for (const auto& spec : kSpecs) {
    const QString tip = QString::fromUtf8(spec.tip);
    const QIcon icon(view_icon_path(QString::fromUtf8(spec.file)));
    auto* act = view_toolbar_->addAction(icon, tip);
    act->setToolTip(tip);
    const char face = spec.face;
    connect(act, &QAction::triggered, this,
            [this, face] { apply_standard_view(face); });
  }

  view_toolbar_->addSeparator();
  act_ortho_ = view_toolbar_->addAction(QStringLiteral("正交"));
  act_ortho_->setCheckable(true);
  act_ortho_->setToolTip(QStringLiteral("正交投影"));
  if (Camera* cam = world_.main_camera()) {
    act_ortho_->setChecked(cam->ortho);
  }
  connect(act_ortho_, &QAction::toggled, this, [this](bool on) {
    if (Camera* cam = world_.main_camera()) {
      cam->ortho = on;
      if (view_cube_) view_cube_->update();
      if (vulkan_window_) vulkan_window_->requestUpdate();
    }
  });
}

void MainWindow::setup_property_dock() {
  property_dock_ = new QDockWidget(QStringLiteral("属性"), this);
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
        if (vulkan_window_) vulkan_window_->requestUpdate();
        document_.mark_dirty();
        refresh_window_title();

        // Bridge FeatureHistory ↔ DocumentHistory for Ctrl+Z.
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
                  if (vulkan_window_) vulkan_window_->requestUpdate();
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
                  if (vulkan_window_) vulkan_window_->requestUpdate();
                  update_property_panel(
                      ecs::selected_entity(world_.registry()));
                  refresh_edit_actions();
                },
        });
        refresh_edit_actions();
        update_property_panel(ecs::selected_entity(world_.registry()));
      });

  auto* view_menu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
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

void MainWindow::refresh_window_title() {
  setWindowTitle(document_.window_title());
}

void MainWindow::rebind_view_cube_camera() {
  if (view_cube_) view_cube_->set_camera(world_.main_camera());
  if (act_ortho_) {
    if (Camera* cam = world_.main_camera()) {
      const QSignalBlocker block(act_ortho_);
      act_ortho_->setChecked(cam->ortho);
    }
  }
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  if (view_cube_) {
    view_cube_->show();
    place_view_cube();
  }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  place_view_cube();
}

void MainWindow::moveEvent(QMoveEvent* event) {
  QMainWindow::moveEvent(event);
  place_view_cube();
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
  if (!view_cube_) return;
  if (event->type() == QEvent::WindowStateChange) {
    if (isMinimized()) {
      view_cube_->hide();
    } else {
      view_cube_->show();
      place_view_cube();
    }
  }
}

void MainWindow::place_view_cube() {
  if (!view_cube_ || !viewport_container_) return;
  constexpr int margin = 10;
  const QPoint global = viewport_container_->mapToGlobal(
      QPoint(viewport_container_->width() - view_cube_->width() - margin,
             margin));
  view_cube_->move(global);
}

void MainWindow::apply_wheel_zoom(int dy) {
  if (dy == 0) return;
  Camera* cam = world_.main_camera();
  if (!cam) return;
  cam->zoom(dy > 0 ? 1.0f : -1.0f);
  if (vulkan_window_) vulkan_window_->requestUpdate();
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape && command_manager_.has_active_tool()) {
    auto ctx = make_command_context();
    command_manager_.cancel_active_tool(ctx);
    sync_tool_ui();
    event->accept();
    return;
  }
  QMainWindow::keyPressEvent(event);
}

bool MainWindow::handle_tool_mouse(QEvent* event) {
  if (!command_manager_.has_active_tool() || !viewport_container_) return false;

  const QEvent::Type type = event->type();
  if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease &&
      type != QEvent::MouseMove) {
    return false;
  }

  auto* e = static_cast<QMouseEvent*>(event);
  const QPoint local =
      viewport_container_->mapFromGlobal(e->globalPosition().toPoint());
  if (!viewport_container_->rect().contains(local)) return false;

  // Scale container (logical) → Vulkan window pixels when they differ (DPI).
  const int cw = std::max(1, viewport_container_->width());
  const int ch = std::max(1, viewport_container_->height());
  const int vw =
      vulkan_window_ ? std::max(1, vulkan_window_->width()) : cw;
  const int vh =
      vulkan_window_ ? std::max(1, vulkan_window_->height()) : ch;
  const float sx = float(local.x()) * float(vw) / float(cw);
  const float sy = float(local.y()) * float(vh) / float(ch);

  auto ctx = make_command_context();
  if (type == QEvent::MouseButtonPress) {
    if (e->button() != Qt::LeftButton) return false;
    if (command_manager_.tool_mouse_press(ctx, sx, sy, int(e->button()))) {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(world_.registry()));
      return true;
    }
    return false;
  }

  if (type == QEvent::MouseButtonRelease) {
    // Swallow left-release so VulkanWindow cannot unsetCursor / start select.
    if (e->button() != Qt::LeftButton) return false;
    sync_tool_ui();
    return true;
  }

  // MouseMove: update rubber-band. Only consume when not panning with RMB/MMB,
  // otherwise orbit/pan handlers never see the drag.
  command_manager_.tool_mouse_move(ctx, sx, sy);
  if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) return false;
  return true;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  // Prefer the viewport filter; also handle via qApp so QWindow-direct events
  // still reach the active tool. Only consume when the cursor is over the view.
  if (watched == viewport_container_ || watched == qApp) {
    if (handle_tool_mouse(event)) return true;
  }

  if (event->type() == QEvent::KeyPress) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Escape && command_manager_.has_active_tool()) {
      auto ctx = make_command_context();
      command_manager_.cancel_active_tool(ctx);
      sync_tool_ui();
      return true;
    }
  }

  if (event->type() == QEvent::Wheel && vulkan_window_ && viewport_container_) {
    auto* we = static_cast<QWheelEvent*>(event);
    const QPoint global = we->globalPosition().toPoint();
    const QRect viewport_global(
        viewport_container_->mapToGlobal(QPoint(0, 0)),
        viewport_container_->size());
    if (!viewport_global.contains(global)) {
      return QMainWindow::eventFilter(watched, event);
    }
    if (view_cube_ && view_cube_->isVisible()) {
      const QRect cube_global(view_cube_->pos(), view_cube_->size());
      if (cube_global.contains(global)) {
        return QMainWindow::eventFilter(watched, event);
      }
    }
    const int dy = wheel_delta_y(we);
    if (dy != 0) {
      apply_wheel_zoom(dy);
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
