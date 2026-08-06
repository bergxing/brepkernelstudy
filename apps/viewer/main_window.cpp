#include "main_window.hpp"

#include "brep/log.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStatusBar>
#include <QVersionNumber>
#include <QVulkanInstance>
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

  QWidget* container = QWidget::createWindowContainer(vulkan_window_, this);
  container->setFocusPolicy(Qt::StrongFocus);
  container->setMouseTracking(true);
  container->installEventFilter(vulkan_window_);
  container->setFocus();
  setCentralWidget(container);
  statusBar()->showMessage(
      QStringLiteral(
          "ECS | Wood | Left-drag: rotate | Right/Middle-drag: pan | Wheel: zoom"));
}

}  // namespace brep::viewer
