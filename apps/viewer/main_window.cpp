#include "main_window.hpp"

#include "brep/brep.hpp"
#include "brep/log.hpp"
#include "commands/command_palette.hpp"
#include "ecs/components.hpp"
#include "ecs/systems.hpp"
#include "view_mdi_subwindow.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QPushButton>
#include <QCoreApplication>
#include <QCursor>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFileInfo>
#include <QHoverEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
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

QString title_for_standard_view(char face) {
  switch (face) {
    case 'f':
      return QStringLiteral("前视图");
    case 'k':
      return QStringLiteral("后视图");
    case 'l':
      return QStringLiteral("左视图");
    case 'r':
      return QStringLiteral("右视图");
    case 't':
      return QStringLiteral("顶视图");
    case 'b':
      return QStringLiteral("底视图");
    case 'h':
      return QStringLiteral("轴侧视图");
    default:
      return QStringLiteral("视图");
  }
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

  document_.new_blank_document(world_);
  BREP_INFO("ECS scene ready: blank Document + Part + camera");

  mdi_area_ = new QMdiArea(this);
  mdi_area_->setViewMode(QMdiArea::SubWindowView);
  mdi_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mdi_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mdi_area_->setTabsClosable(true);
  setCentralWidget(mdi_area_);
  setMouseTracking(true);
  mdi_area_->installEventFilter(this);
  if (mdi_area_->viewport()) {
    mdi_area_->viewport()->installEventFilter(this);
  }

  connect(mdi_area_, &QMdiArea::subWindowActivated, this,
          &MainWindow::on_sub_window_activated);

  create_view_window(title_for_standard_view('h'), 'h');

  view_cube_ = new ViewCubeWidget(this);
  view_cube_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                             Qt::WindowDoesNotAcceptFocus);
  view_cube_->setAttribute(Qt::WA_ShowWithoutActivating);
  rebind_view_cube_camera();
  view_cube_->set_redraw_callback([this] {
    if (auto* vw = active_vulkan_window()) vw->requestUpdate();
    if (auto* container = active_viewport_container()) {
      container->setFocus(Qt::OtherFocusReason);
    }
  });
  place_view_cube();
  view_cube_->hide();

  setup_cursor_tip();
  setup_menus();
  setup_toolbar();
  setup_view_toolbar();
  setup_property_dock();
  refresh_window_title();
  refresh_edit_actions();
  update_property_panel(entt::null);

  qApp->installEventFilter(this);

  statusBar()->showMessage(QStringLiteral(
      "XCAD | 左键选择/框选 | Ctrl+追加 | 右键菜单 | "
      "中键平移 | Ctrl+中键旋转 | 双击中键缩放到全部 | ESC 取消工具"));
}

MainWindow::~MainWindow() {
  BREP_INFO("MainWindow::~MainWindow begin");
  // Must remove before QObject teardown; otherwise quit-time events can call
  // into a destroyed MainWindow (ACCESS_VIOLATION / 0xC0000005).
  if (qApp) qApp->removeEventFilter(this);
  if (tool_cursor_overridden_) {
    QApplication::restoreOverrideCursor();
    tool_cursor_overridden_ = false;
  }

  // QObject children (MDI / QVulkanWindow) are destroyed in ~QObject, AFTER
  // member unique_ptrs. QVulkanInstance must outlive every QVulkanWindow, so
  // destroy Vulkan views here before vulkan_instance_ is reset/destroyed.
  suppress_ensure_view_ = true;
  if (view_cube_) {
    BREP_INFO("MainWindow::~MainWindow: destroy ViewCube");
    view_cube_->set_redraw_callback({});
    view_cube_->set_camera(nullptr);
    delete view_cube_;
    view_cube_ = nullptr;
  }
  if (mdi_area_) {
    BREP_INFO("MainWindow::~MainWindow: destroy {} MDI subwindow(s)",
              mdi_area_->subWindowList().size());
    const auto subs = mdi_area_->subWindowList();
    for (auto* sub : subs) {
      if (!sub) continue;
      BREP_INFO("MainWindow::~MainWindow: delete subwindow '{}'",
                sub->windowTitle().toStdString());
      mdi_area_->removeSubWindow(sub);
      delete sub;
    }
    view_windows_.clear();
    BREP_INFO("MainWindow::~MainWindow: delete QMdiArea");
    setCentralWidget(nullptr);
    delete mdi_area_;
    mdi_area_ = nullptr;
  }

  BREP_INFO("MainWindow::~MainWindow: reset QVulkanInstance");
  vulkan_instance_.reset();
  BREP_INFO("MainWindow::~MainWindow end");
}

bool MainWindow::open_document(const QString& path) {
  auto ctx = make_command_context();
  const auto result = commands::open_xl_file(ctx, path);
  if (!result.succeeded()) {
    if (!result.message.isEmpty()) {
      statusBar()->showMessage(result.message, 6000);
    }
    return false;
  }

  if (auto* vw = active_vulkan_window()) {
    const float aspect =
        float(std::max(1, vw->width())) / float(std::max(1, vw->height()));
    ecs::fit_camera_to_scene(world_.registry(), vw->camera(), aspect);
  }
  update_property_panel(entt::null);
  request_all_views_update();
  refresh_window_title();
  statusBar()->showMessage(result.message, 6000);
  return true;
}

void MainWindow::wire_vulkan_window(VulkanWindow* window) {
  window->set_selection_callback([this](entt::entity entity) {
    update_property_panel(entity);
    request_all_views_update();
    const std::size_t count = ecs::selected_count(world_.registry());
    if (count == 0) {
      statusBar()->showMessage(QStringLiteral("已取消选择"), 3000);
      return;
    }
    if (count > 1) {
      statusBar()->showMessage(
          QStringLiteral("已多选: %1 个对象").arg(count), 6000);
      return;
    }
    const std::string label =
        ecs::selection_label(world_.registry(), entity);
    statusBar()->showMessage(
        QStringLiteral("已选中: %1")
            .arg(QString::fromStdString(label)),
        6000);
  });
  window->set_tool_motion_callback([this](float x, float y) {
    if (!command_manager_.has_active_tool()) return;
    auto ctx = make_command_context();
    command_manager_.tool_mouse_move(ctx, x, y);
  });
  window->set_tool_press_callback([this](float x, float y, int button) {
    if (!command_manager_.has_active_tool()) return false;
    auto ctx = make_command_context();
    const bool consumed =
        command_manager_.tool_mouse_press(ctx, x, y, button);
    sync_tool_ui();
    refresh_edit_actions();
    return consumed;
  });
  window->set_context_menu_callback([this, window](float x, float y) {
    show_viewport_context_menu(window, x, y);
  });
}

void MainWindow::clear_view_fill_states() {
  for (auto* sub : mdi_area_ ? mdi_area_->subWindowList()
                             : QList<QMdiSubWindow*>{}) {
    if (auto* view = qobject_cast<ViewMdiSubWindow*>(sub)) {
      view->clear_fill();
    }
  }
}

VulkanWindow* MainWindow::create_view_window(const QString& title,
                                             char standard_view,
                                             bool fill_workspace) {
  auto* vulkan_window = new VulkanWindow();
  vulkan_window->setVulkanInstance(vulkan_instance_.get());
  vulkan_window->setSampleCount(1);
  vulkan_window->set_world(&world_);
  vulkan_window->camera().set_standard_view(standard_view);
  // Avoid leaking the QWindow title into the MDI caption.
  vulkan_window->setTitle(QString());
  wire_vulkan_window(vulkan_window);
  if (command_manager_.has_active_tool()) {
    vulkan_window->set_selection_enabled(
        command_manager_.active_tool_allows_selection());
  }

  // Host wraps the native Vulkan container so the box-select overlay can
  // paint above it (children of createWindowContainer stay hidden under GL).
  auto* host = new QWidget();
  host->setMinimumSize(160, 120);
  host->setFocusPolicy(Qt::StrongFocus);
  host->setMouseTracking(true);
  host->setAttribute(Qt::WA_Hover, true);
  host->setCursor(Qt::ArrowCursor);

  auto* container = QWidget::createWindowContainer(vulkan_window, host);
  container->setFocusPolicy(Qt::StrongFocus);
  container->setMouseTracking(true);
  container->setAttribute(Qt::WA_Hover, true);
  // Break inheritance from QMdiSubWindow border resize cursors.
  container->setCursor(Qt::ArrowCursor);
  auto* layout = new QVBoxLayout(host);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(container);

  vulkan_window->set_rubber_band_host(host);
  container->installEventFilter(vulkan_window);
  container->installEventFilter(this);
  host->installEventFilter(this);

  auto* sub = new ViewMdiSubWindow(mdi_area_);
  sub->setWidget(host);
  mdi_area_->addSubWindow(sub);
  const QString numbered =
      title.isEmpty()
          ? QStringLiteral("视图 %1").arg(++view_serial_)
          : QStringLiteral("%1 (%2)").arg(title).arg(++view_serial_);
  sub->setWindowTitle(numbered);
  sub->installEventFilter(this);
  view_windows_.insert(sub, vulkan_window);

  connect(sub, &QObject::destroyed, this, [this, sub] {
    view_windows_.remove(sub);
    ensure_minimum_view();
  });

  sub->resize(720, 480);
  sub->show();
  mdi_area_->setActiveSubWindow(sub);
  if (fill_workspace) {
    // Defer until the MDI viewport has a real size.
    QTimer::singleShot(0, this, [this, sub] {
      if (!sub) return;
      sub->fill_workspace();
      place_view_cube();
    });
  }
  host->setFocus();
  rebind_view_cube_camera();
  place_view_cube();
  return vulkan_window;
}

VulkanWindow* MainWindow::vulkan_window_for_sub(QMdiSubWindow* sub) const {
  if (!sub) return nullptr;
  return view_windows_.value(sub, nullptr);
}

VulkanWindow* MainWindow::active_vulkan_window() const {
  if (!mdi_area_) return nullptr;
  return vulkan_window_for_sub(mdi_area_->activeSubWindow());
}

QWidget* MainWindow::active_viewport_container() const {
  if (!mdi_area_) return nullptr;
  if (auto* sub = mdi_area_->activeSubWindow()) return sub->widget();
  return nullptr;
}

VulkanWindow* MainWindow::vulkan_window_at_global(
    const QPoint& global) const {
  if (!mdi_area_) return nullptr;
  for (auto* sub : mdi_area_->subWindowList()) {
    QWidget* container = sub->widget();
    if (!container || !container->isVisible()) continue;
    const QRect rect(container->mapToGlobal(QPoint(0, 0)), container->size());
    if (rect.contains(global)) {
      return vulkan_window_for_sub(sub);
    }
  }
  return nullptr;
}

void MainWindow::request_all_views_update() {
  for (auto* window : view_windows_) {
    if (window) window->requestUpdate();
  }
}

void MainWindow::ensure_minimum_view() {
  if (suppress_ensure_view_ || !mdi_area_) return;
  if (!view_windows_.isEmpty() || !mdi_area_->subWindowList().isEmpty()) {
    return;
  }
  create_view_window(title_for_standard_view('h'), 'h');
}

void MainWindow::on_sub_window_activated(QMdiSubWindow* sub) {
  Q_UNUSED(sub);
  rebind_view_cube_camera();
  place_view_cube();
}

void MainWindow::on_new_view() {
  clear_view_fill_states();
  create_view_window(title_for_standard_view('h'), 'h', false);
  mdi_area_->tileSubWindows();
  place_view_cube();
}

void MainWindow::on_quad_views() {
  // Replace current layout with front / top / right / iso.
  suppress_ensure_view_ = true;
  const auto existing = mdi_area_->subWindowList();
  for (auto* sub : existing) {
    view_windows_.remove(sub);
    sub->removeEventFilter(this);
    sub->close();
  }
  view_windows_.clear();
  suppress_ensure_view_ = false;

  const char faces[] = {'f', 't', 'r', 'h'};
  for (char face : faces) {
    create_view_window(title_for_standard_view(face), face, false);
  }
  clear_view_fill_states();
  mdi_area_->tileSubWindows();
  place_view_cube();
}

void MainWindow::on_tile_views() {
  clear_view_fill_states();
  if (mdi_area_) mdi_area_->tileSubWindows();
  place_view_cube();
}

void MainWindow::on_cascade_views() {
  clear_view_fill_states();
  if (mdi_area_) mdi_area_->cascadeSubWindows();
  place_view_cube();
}

void MainWindow::on_close_active_view() {
  if (!mdi_area_) return;
  if (mdi_area_->subWindowList().size() <= 1) {
    statusBar()->showMessage(QStringLiteral("至少保留一个视图窗口"), 3000);
    return;
  }
  if (auto* sub = mdi_area_->activeSubWindow()) {
    sub->close();
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
  auto* vw = active_vulkan_window();
  if (!vw) return;
  vw->camera().set_standard_view(face);
  if (act_ortho_) {
    const QSignalBlocker block(act_ortho_);
    act_ortho_->setChecked(vw->camera().ortho);
  }
  if (view_cube_) view_cube_->update();
  vw->requestUpdate();
}

commands::CommandContext MainWindow::make_command_context() {
  commands::CommandContext ctx;
  ctx.world = &world_;
  ctx.session = &document_;
  ctx.history = &command_manager_.history();
  ctx.parent_widget = this;
  ctx.wood_albedo_path = wood_albedo_path().toStdString();

  auto* vw = active_vulkan_window();
  auto* container = active_viewport_container();
  ctx.viewport = vw;
  ctx.view_camera = vw ? &vw->camera() : nullptr;

  if (vw && vw->width() > 0 && vw->height() > 0) {
    ctx.viewport_w = vw->width();
    ctx.viewport_h = vw->height();
  } else if (container) {
    ctx.viewport_w = std::max(1, container->width());
    ctx.viewport_h = std::max(1, container->height());
  }

  ctx.report_status = [this](const QString& msg) {
    statusBar()->showMessage(msg);
    refresh_cursor_tip();
  };
  ctx.request_redraw = [this] { request_all_views_update(); };
  ctx.after_document_reset = [this] {
    rebind_view_cube_camera();
    refresh_window_title();
    update_property_panel(entt::null);
    request_all_views_update();
  };
  ctx.refresh_ui = [this] {
    refresh_window_title();
    refresh_edit_actions();
  };
  ctx.set_preview_edges = [this](EdgeMesh edges) {
    if (auto* active = active_vulkan_window()) {
      active->set_preview_edges(std::move(edges));
    }
  };
  ctx.set_preview = [this](EdgeMesh edges, TriangleMesh solid) {
    if (auto* active = active_vulkan_window()) {
      active->set_preview(std::move(edges), std::move(solid));
    }
  };
  ctx.clear_preview = [this] {
    if (auto* active = active_vulkan_window()) {
      active->clear_preview();
    }
  };
  return ctx;
}

void MainWindow::sync_tool_ui() {
  const bool tool = command_manager_.has_active_tool();
  const bool allow_sel = command_manager_.active_tool_allows_selection();
  for (auto* window : view_windows_) {
    if (window) window->set_selection_enabled(!tool || allow_sel);
  }

  // Crosshair only while the tool owns picking (base / place), not during
  // object selection.
  const bool want_cross = tool && !allow_sel;
  if (want_cross && !tool_cursor_overridden_) {
    QApplication::setOverrideCursor(Qt::CrossCursor);
    tool_cursor_overridden_ = true;
  } else if (!want_cross && tool_cursor_overridden_) {
    QApplication::restoreOverrideCursor();
    tool_cursor_overridden_ = false;
  }
  refresh_cursor_tip();
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

void MainWindow::setup_cursor_tip() {
  cursor_tip_ = new QLabel(this);
  cursor_tip_->setObjectName(QStringLiteral("CursorTip"));
  cursor_tip_->setAttribute(Qt::WA_TransparentForMouseEvents);
  cursor_tip_->setAttribute(Qt::WA_ShowWithoutActivating);
  cursor_tip_->setFocusPolicy(Qt::NoFocus);
  cursor_tip_->setStyleSheet(QStringLiteral(
      "QLabel#CursorTip {"
      "  background-color: rgba(28, 28, 28, 210);"
      "  color: #f2f2f2;"
      "  border: 1px solid rgba(255, 255, 255, 45);"
      "  border-radius: 3px;"
      "  padding: 3px 8px;"
      "  font-size: 12px;"
      "}"));
  cursor_tip_->hide();
}

void MainWindow::hide_cursor_tip() {
  if (cursor_tip_) cursor_tip_->hide();
}

bool MainWindow::map_global_to_viewport(const QPoint& global,
                                        VulkanWindow*& out_window, float& out_x,
                                        float& out_y) const {
  out_window = nullptr;
  auto* vw = vulkan_window_at_global(global);
  if (!vw) return false;

  QWidget* container = nullptr;
  for (auto it = view_windows_.constBegin(); it != view_windows_.constEnd();
       ++it) {
    if (it.value() == vw) {
      container = it.key() ? it.key()->widget() : nullptr;
      break;
    }
  }
  if (!container) return false;

  const QPoint local = container->mapFromGlobal(global);
  if (!container->rect().contains(local)) return false;

  const int cw = std::max(1, container->width());
  const int ch = std::max(1, container->height());
  const int vww = std::max(1, vw->width());
  const int vwh = std::max(1, vw->height());
  out_window = vw;
  out_x = float(local.x()) * float(vww) / float(cw);
  out_y = float(local.y()) * float(vwh) / float(ch);
  return true;
}

QString MainWindow::resolve_cursor_tip_text(VulkanWindow* window, float x,
                                            float y) {
  (void)window;
  (void)x;
  (void)y;

  // Only show tips while an interactive tool is active.
  // Idle / pan / orbit / box-select stay silent.
  if (command_manager_.has_active_tool()) {
    QString prompt = command_manager_.active_prompt();
    prompt.remove(QStringLiteral(" (ESC 取消)"));
    if (!prompt.isEmpty()) return prompt;
  }

  return {};
}

void MainWindow::update_cursor_tip_at_global(const QPoint& global) {
  if (!cursor_tip_) return;

  if (view_cube_ && view_cube_->isVisible()) {
    const QRect cube_global(view_cube_->pos(), view_cube_->size());
    if (cube_global.contains(global)) {
      hide_cursor_tip();
      return;
    }
  }

  VulkanWindow* vw = nullptr;
  float x = 0.0f;
  float y = 0.0f;
  if (!map_global_to_viewport(global, vw, x, y)) {
    hide_cursor_tip();
    return;
  }

  const QString text = resolve_cursor_tip_text(vw, x, y);
  if (text.isEmpty()) {
    hide_cursor_tip();
    return;
  }

  cursor_tip_->setText(text);
  cursor_tip_->adjustSize();

  constexpr int kOffset = 16;
  QPoint pos = mapFromGlobal(global + QPoint(kOffset, kOffset));
  const QRect bounds = rect().adjusted(4, 4, -4, -4);
  const int max_x = std::max(bounds.left(), bounds.right() - cursor_tip_->width());
  const int max_y =
      std::max(bounds.top(), bounds.bottom() - cursor_tip_->height());
  pos.setX(std::clamp(pos.x(), bounds.left(), max_x));
  pos.setY(std::clamp(pos.y(), bounds.top(), max_y));

  cursor_tip_->move(pos);
  cursor_tip_->show();
  cursor_tip_->raise();
}

void MainWindow::refresh_cursor_tip() {
  update_cursor_tip_at_global(QCursor::pos());
}

void MainWindow::show_viewport_context_menu(VulkanWindow* window, float x,
                                           float y) {
  if (!window || command_manager_.has_active_tool()) return;

  auto& registry = world_.registry();
  const QSize sz = window->size();
  const entt::entity hit = ecs::pick_renderable(
      registry, window->camera(), sz.width(), sz.height(), x, y);

  const bool on_object = hit != entt::null;
  if (on_object) {
    // Clicked an object: select it unless it is already in the selection.
    if (!registry.all_of<ecs::SelectedTag>(hit)) {
      ecs::set_selection(registry, hit);
      update_property_panel(ecs::selected_entity(registry));
      request_all_views_update();
    }
  }

  hide_cursor_tip();

  QMenu menu(this);
  auto* act_copy = menu.addAction(QStringLiteral("复制"));
  menu.addSeparator();
  auto* act_undo = menu.addAction(QStringLiteral("撤销"));
  auto* act_redo = menu.addAction(QStringLiteral("重做"));
  act_undo->setEnabled(command_manager_.history().can_undo());
  act_redo->setEnabled(command_manager_.history().can_redo());

  QAction* chosen = menu.exec(QCursor::pos());
  refresh_cursor_tip();
  if (!chosen) return;

  if (chosen == act_copy) {
    if (!on_object) {
      // Empty space → force "copy then select" path.
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

void MainWindow::setup_window_menu() {
  auto* window_menu = menuBar()->addMenu(QStringLiteral("窗口(&W)"));

  auto* act_new = window_menu->addAction(QStringLiteral("新建视图(&N)"));
  act_new->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
  connect(act_new, &QAction::triggered, this, &MainWindow::on_new_view);

  auto* act_quad = window_menu->addAction(QStringLiteral("四视图(&Q)"));
  connect(act_quad, &QAction::triggered, this, &MainWindow::on_quad_views);

  window_menu->addSeparator();

  auto* act_tile = window_menu->addAction(QStringLiteral("平铺(&T)"));
  connect(act_tile, &QAction::triggered, this, &MainWindow::on_tile_views);

  auto* act_cascade = window_menu->addAction(QStringLiteral("层叠(&C)"));
  connect(act_cascade, &QAction::triggered, this, &MainWindow::on_cascade_views);

  window_menu->addSeparator();

  auto* act_close =
      window_menu->addAction(QStringLiteral("关闭当前视图(&L)"));
  connect(act_close, &QAction::triggered, this,
          &MainWindow::on_close_active_view);
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

  edit_menu->addSeparator();
  auto* act_copy = edit_menu->addAction(QStringLiteral("复制(&C)…"));
  act_copy->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+C")));
  act_copy->setToolTip(
      QStringLiteral("复制选中立方体：基点 → 放置点"));
  bind_action(act_copy, "edit.copy");

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

  setup_window_menu();
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

  auto* act_copy = toolbar_->addAction(QStringLiteral("复制"));
  act_copy->setToolTip(QStringLiteral("复制选中对象 (Ctrl+Shift+C)"));
  bind_action(act_copy, "edit.copy");

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

bool MainWindow::confirm_close_or_save() {
  BREP_INFO("confirm_close_or_save dirty={} tool={}", document_.dirty(),
            command_manager_.has_active_tool());
  if (command_manager_.has_active_tool()) {
    auto ctx = make_command_context();
    command_manager_.cancel_active_tool(ctx);
    sync_tool_ui();
  }

  if (!document_.dirty()) {
    BREP_INFO("confirm_close_or_save: clean document, allow close");
    return true;
  }

  QMessageBox box(this);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(QStringLiteral("保存文档"));
  box.setText(QStringLiteral("文档“%1”已修改，是否保存？")
                  .arg(document_.title()));
  box.setInformativeText(
      QStringLiteral("选择“保存”写入文件后退出；“不保存”直接退出；"
                     "“取消”继续编辑。"));
  QAbstractButton* btn_save =
      box.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
  QAbstractButton* btn_discard =
      box.addButton(QStringLiteral("不保存"), QMessageBox::DestructiveRole);
  QAbstractButton* btn_cancel =
      box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
  box.setDefaultButton(qobject_cast<QPushButton*>(btn_save));
  box.setEscapeButton(btn_cancel);
  box.exec();

  if (box.clickedButton() == btn_cancel) {
    BREP_INFO("confirm_close_or_save: user cancelled");
    return false;
  }
  if (box.clickedButton() == btn_discard) {
    BREP_INFO("confirm_close_or_save: discard changes");
    return true;
  }

  // Save then close. Abort close if the user cancels the save dialog / fails.
  BREP_INFO("confirm_close_or_save: saving before close");
  const auto result = run_command("file.save");
  BREP_INFO("confirm_close_or_save: save status={}", int(result.status));
  return result.succeeded();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  BREP_INFO("MainWindow::closeEvent");
  if (!confirm_close_or_save()) {
    BREP_INFO("MainWindow::closeEvent ignored");
    event->ignore();
    return;
  }

  // Tear down app-wide hooks before child/Vulkan destruction churn.
  if (qApp) qApp->removeEventFilter(this);
  hide_cursor_tip();
  if (view_cube_) {
    view_cube_->hide();
    view_cube_->set_camera(nullptr);
  }
  for (auto* window : view_windows_) {
    if (!window) continue;
    window->set_selection_callback({});
    window->set_tool_motion_callback({});
    window->set_tool_press_callback({});
    window->set_context_menu_callback({});
    window->set_world(nullptr);
  }
  BREP_INFO("MainWindow::closeEvent accepted");
  event->accept();
}

void MainWindow::rebind_view_cube_camera() {
  auto* vw = active_vulkan_window();
  if (view_cube_) view_cube_->set_camera(vw ? &vw->camera() : nullptr);
  if (act_ortho_ && vw) {
    const QSignalBlocker block(act_ortho_);
    act_ortho_->setChecked(vw->camera().ortho);
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

bool MainWindow::is_view_layout_object(const QObject* watched) const {
  if (!watched || !mdi_area_) return false;
  if (watched == mdi_area_ || watched == mdi_area_->viewport()) return true;
  for (auto it = view_windows_.cbegin(); it != view_windows_.cend(); ++it) {
    if (watched == it.key() || watched == it.key()->widget()) return true;
  }
  return false;
}

void MainWindow::place_view_cube() {
  if (!view_cube_) return;

  QWidget* container = active_viewport_container();
  QMdiSubWindow* sub = mdi_area_ ? mdi_area_->activeSubWindow() : nullptr;
  if (!container || !sub || sub->isMinimized() || !container->isVisible() ||
      isMinimized()) {
    view_cube_->hide();
    return;
  }

  constexpr int margin = 10;
  // Anchor to the active viewport's top-right corner (client area).
  const QPoint global = container->mapToGlobal(
      QPoint(container->width() - view_cube_->width() - margin, margin));
  view_cube_->move(global);
  if (!view_cube_->isVisible()) view_cube_->show();
}

void MainWindow::apply_wheel_zoom(VulkanWindow* window, int dy) {
  if (!window || dy == 0) return;
  window->handle_wheel(dy);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
  if (command_manager_.has_active_tool()) {
    if (event->key() == Qt::Key_Escape) {
      auto ctx = make_command_context();
      command_manager_.cancel_active_tool(ctx);
      sync_tool_ui();
      event->accept();
      return;
    }
    auto ctx = make_command_context();
    if (command_manager_.tool_key_press(ctx, int(event->key()))) {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(world_.registry()));
      event->accept();
      return;
    }
  }
  QMainWindow::keyPressEvent(event);
}

bool MainWindow::handle_tool_mouse(QEvent* event) {
  if (!command_manager_.has_active_tool()) return false;
  // Selection phase (e.g. Copy): let the viewport handle pick / box / Ctrl.
  if (command_manager_.active_tool_allows_selection()) return false;

  const QEvent::Type type = event->type();
  if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonRelease &&
      type != QEvent::MouseMove) {
    return false;
  }

  auto* e = static_cast<QMouseEvent*>(event);
  const QPoint global = e->globalPosition().toPoint();
  auto* vw = vulkan_window_at_global(global);
  if (!vw) return false;

  // Activate the viewport under the cursor so picking uses its camera.
  for (auto it = view_windows_.begin(); it != view_windows_.end(); ++it) {
    if (it.value() == vw) {
      if (mdi_area_->activeSubWindow() != it.key()) {
        mdi_area_->setActiveSubWindow(it.key());
      }
      break;
    }
  }

  QWidget* container = active_viewport_container();
  if (!container) return false;

  const QPoint local = container->mapFromGlobal(global);
  if (!container->rect().contains(local)) return false;

  const int cw = std::max(1, container->width());
  const int ch = std::max(1, container->height());
  const int vww = std::max(1, vw->width());
  const int vwh = std::max(1, vw->height());
  const float sx = float(local.x()) * float(vww) / float(cw);
  const float sy = float(local.y()) * float(vwh) / float(ch);

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
    if (e->button() != Qt::LeftButton) return false;
    sync_tool_ui();
    return true;
  }

  command_manager_.tool_mouse_move(ctx, sx, sy);
  if (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) return false;
  return true;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::MouseMove ||
      event->type() == QEvent::HoverMove) {
    QPoint global;
    if (event->type() == QEvent::MouseMove) {
      global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
    } else {
      global = static_cast<QHoverEvent*>(event)->globalPosition().toPoint();
    }
    update_cursor_tip_at_global(global);
  } else if (event->type() == QEvent::Leave) {
    if (qobject_cast<QWidget*>(watched)) {
      // Leaving a viewport container / MDI child — hide if cursor left views.
      if (!vulkan_window_at_global(QCursor::pos())) {
        hide_cursor_tip();
      }
    }
  }

  // Block closing the last MDI view subwindow.
  if (event->type() == QEvent::Close) {
    auto* sub = qobject_cast<QMdiSubWindow*>(watched);
    if (sub && mdi_area_ && view_windows_.contains(sub)) {
      if (mdi_area_->subWindowList().size() <= 1) {
        statusBar()->showMessage(QStringLiteral("至少保留一个视图窗口"), 3000);
        event->ignore();
        return true;
      }
    }
  }

  // Keep ViewCube glued to the active viewport when MDI windows move/resize.
  switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Hide:
    case QEvent::WindowStateChange:
    case QEvent::LayoutRequest:
      if (is_view_layout_object(watched)) {
        place_view_cube();
      }
      break;
    default:
      break;
  }

  if (handle_tool_mouse(event)) return true;

  if (event->type() == QEvent::KeyPress &&
      command_manager_.has_active_tool()) {
    auto* ke = static_cast<QKeyEvent*>(event);
    if (ke->key() == Qt::Key_Escape) {
      auto ctx = make_command_context();
      command_manager_.cancel_active_tool(ctx);
      sync_tool_ui();
      return true;
    }
    auto ctx = make_command_context();
    if (command_manager_.tool_key_press(ctx, int(ke->key()))) {
      sync_tool_ui();
      refresh_edit_actions();
      update_property_panel(ecs::selected_entity(world_.registry()));
      return true;
    }
  }

  if (event->type() == QEvent::Wheel) {
    auto* we = static_cast<QWheelEvent*>(event);
    const QPoint global = we->globalPosition().toPoint();
    if (view_cube_ && view_cube_->isVisible()) {
      const QRect cube_global(view_cube_->pos(), view_cube_->size());
      if (cube_global.contains(global)) {
        return QMainWindow::eventFilter(watched, event);
      }
    }
    if (auto* vw = vulkan_window_at_global(global)) {
      const int dy = wheel_delta_y(we);
      if (dy != 0) {
        apply_wheel_zoom(vw, dy);
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
