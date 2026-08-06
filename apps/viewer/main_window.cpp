#include "main_window.hpp"

#include "brep/log.hpp"

#include <QApplication>
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
namespace {

int wheel_delta_y(const QWheelEvent* event) {
  // Prefer angleDelta (mouse wheel notches). Fall back to pixelDelta (touchpad).
  if (event->angleDelta().y() != 0) return event->angleDelta().y();
  if (event->pixelDelta().y() != 0) return event->pixelDelta().y();
  return 0;
}

}  // namespace

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

  // Catch wheel at the application level — QVulkanWindow / createWindowContainer
  // often drops wheel events before they reach the QWindow on Windows.
  qApp->installEventFilter(this);

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

void MainWindow::apply_wheel_zoom(int dy) {
  if (dy == 0) return;

  // Mutate the ECS camera directly (same object the renderer reads every frame).
  Camera* cam = world_.main_camera();
  if (!cam) {
    BREP_ERROR("wheel zoom: main camera missing");
    return;
  }

  const float dist0 = cam->distance;
  const float half0 = cam->ortho_half_h;
  const bool ortho0 = cam->ortho;
  cam->zoom(dy > 0 ? 1.0f : -1.0f);

  BREP_INFO(
      "wheel zoom delta={} ortho={} dist {:.3f}->{:.3f} orthoHalf {:.3f}->{:.3f}",
      dy, ortho0, dist0, cam->distance, half0, cam->ortho_half_h);

  statusBar()->showMessage(
      QStringLiteral("Zoom | dist=%1  orthoHalf=%2  mode=%3")
          .arg(cam->distance, 0, 'f', 2)
          .arg(cam->ortho_half_h, 0, 'f', 2)
          .arg(cam->ortho ? QStringLiteral("ortho")
                          : QStringLiteral("persp")));

  if (vulkan_window_) vulkan_window_->requestUpdate();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Wheel && vulkan_window_ && viewport_container_) {
    auto* we = static_cast<QWheelEvent*>(event);
    const QPoint global = we->globalPosition().toPoint();

    const QRect viewport_global(
        viewport_container_->mapToGlobal(QPoint(0, 0)),
        viewport_container_->size());
    if (!viewport_global.contains(global)) {
      return QMainWindow::eventFilter(watched, event);
    }

    // Let the ViewCube keep its own wheel (none today) / click area alone.
    if (view_cube_ && view_cube_->isVisible()) {
      const QRect cube_global(view_cube_->pos(), view_cube_->size());
      if (cube_global.contains(global)) {
        return QMainWindow::eventFilter(watched, event);
      }
    }

    const int dy = wheel_delta_y(we);
    if (dy != 0) {
      apply_wheel_zoom(dy);
      return true;  // consume — avoid double-handling by QWindow/container
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
