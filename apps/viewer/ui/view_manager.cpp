#include "main_window.hpp"

#include "ecs/components.hpp"
#include "ecs/systems.hpp"
#include "view_mdi_subwindow.hpp"

#include <QApplication>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QRect>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace brep::viewer {

QString MainWindow::title_for_standard_view(char face) const {
  switch (face) {
    case 'f':
      return tr("Front View");
    case 'k':
      return tr("Back View");
    case 'l':
      return tr("Left View");
    case 'r':
      return tr("Right View");
    case 't':
      return tr("Top View");
    case 'b':
      return tr("Bottom View");
    case 'h':
      return tr("Isometric View");
    default:
      return tr("View");
  }
}

QString MainWindow::format_view_title(char standard_view, int serial) const {
  if (standard_view == 0) {
    return tr("View %1").arg(serial);
  }
  return tr("%1 (%2)").arg(title_for_standard_view(standard_view)).arg(serial);
}

void MainWindow::refresh_view_titles() {
  for (auto it = view_windows_.cbegin(); it != view_windows_.cend(); ++it) {
    if (auto* sub = qobject_cast<ViewMdiSubWindow*>(it.key())) {
      sub->setWindowTitle(
          format_view_title(sub->standard_view(), sub->view_serial()));
    }
  }
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

VulkanWindow* MainWindow::create_view_window(char standard_view,
                                             bool fill_workspace) {
  auto* vulkan_window = new VulkanWindow();
  vulkan_window->setVulkanInstance(vulkan_instance_.get());
  vulkan_window->setSampleCount(1);
  vulkan_window->set_world(&world_);
  vulkan_window->camera().set_standard_view(standard_view);
  vulkan_window->setTitle(QString());
  wire_vulkan_window(vulkan_window);
  if (command_manager_.has_active_tool()) {
    vulkan_window->set_selection_enabled(
        command_manager_.active_tool_allows_selection());
  }

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
  const int serial = ++view_serial_;
  sub->set_view_caption(standard_view, serial);
  sub->setWindowTitle(format_view_title(standard_view, serial));
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
  create_view_window('h');
}

void MainWindow::on_sub_window_activated(QMdiSubWindow* sub) {
  Q_UNUSED(sub);
  rebind_view_cube_camera();
  place_view_cube();
}

void MainWindow::on_new_view() {
  clear_view_fill_states();
  create_view_window('h', false);
  mdi_area_->tileSubWindows();
  place_view_cube();
}

void MainWindow::on_quad_views() {
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
    create_view_window(face, false);
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

}  // namespace brep::viewer
