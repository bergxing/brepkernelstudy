#include "main_window.hpp"

#include "brep/log.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QStatusBar>
#include <QVersionNumber>
#include <QVulkanInstance>
#include <QWheelEvent>
#include <QWidget>

#include <stdexcept>

namespace brep::viewer {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("B-Rep Kernel Viewer"));
  resize(1100, 720);

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

  QString wood_path = QStringLiteral(BREP_VIEWER_ASSETS_DIR "/wood.png");
  if (!QFileInfo::exists(wood_path)) {
    wood_path = QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("assets/wood.png"));
  }
  world_.create_demo_box_scene(wood_path.toStdString());
  BREP_INFO("ECS scene ready: camera + demo_box (wood)");

  viewport_container_ = QWidget::createWindowContainer(vulkan_window_, this);
  viewport_container_->setFocusPolicy(Qt::StrongFocus);
  viewport_container_->setMouseTracking(true);
  viewport_container_->installEventFilter(vulkan_window_);
  viewport_container_->setFocus();
  setCentralWidget(viewport_container_);

  // Native Vulkan HWND covers sibling widgets; use a frameless tool window overlay.
  // WindowDoesNotAcceptFocus keeps wheel/keyboard on the viewport after cube clicks.
  view_cube_ = new ViewCubeWidget(this);
  view_cube_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                             Qt::WindowDoesNotAcceptFocus);
  view_cube_->setAttribute(Qt::WA_ShowWithoutActivating);
  view_cube_->set_camera(world_.main_camera());
  view_cube_->set_redraw_callback([this] {
    if (vulkan_window_) vulkan_window_->requestUpdate();
    if (viewport_container_) {
      viewport_container_->setFocus(Qt::OtherFocusReason);
    }
  });
  place_view_cube();
  view_cube_->show();

  // Forward wheel events that land on the main window chrome toward the viewport.
  installEventFilter(this);

  statusBar()->showMessage(QStringLiteral(
      "ECS | Axes + ViewCube | Left-drag: rotate | Right/Middle: pan | Wheel: "
      "zoom | Click cube faces to snap"));
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

void MainWindow::forward_wheel(QWheelEvent* event) {
  if (!vulkan_window_) return;
  int dy = event->angleDelta().y();
  if (dy == 0) dy = event->pixelDelta().y();
  if (dy != 0) vulkan_window_->handle_wheel(dy);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Wheel && watched == this) {
    auto* we = static_cast<QWheelEvent*>(event);
    if (viewport_container_) {
      const QPoint local =
          viewport_container_->mapFromGlobal(we->globalPosition().toPoint());
      if (viewport_container_->rect().contains(local)) {
        forward_wheel(we);
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
