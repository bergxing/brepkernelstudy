#include "main_window.hpp"

#include "brep/log.hpp"
#include "commands/command_palette.hpp"
#include "ecs/systems.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QShowEvent>
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
  refresh_window_title();
  refresh_edit_actions();

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
  toolbar_->setMovable(false);
  toolbar_->setIconSize(QSize(20, 20));

  auto* act_new = toolbar_->addAction(QStringLiteral("新建"));
  bind_action(act_new, "doc.new");

  auto* act_box = toolbar_->addAction(QStringLiteral("立方体"));
  act_box->setToolTip(QStringLiteral("三点创建盒子 (Ctrl+B)"));
  bind_action(act_box, "part.create_box");

  auto* act_export = toolbar_->addAction(QStringLiteral("导出 DXF"));
  bind_action(act_export, "file.export_dxf");
}

void MainWindow::refresh_window_title() {
  setWindowTitle(document_.window_title());
}

void MainWindow::rebind_view_cube_camera() {
  if (view_cube_) view_cube_->set_camera(world_.main_camera());
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
