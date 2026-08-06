#include "main_window.hpp"

#include "brep/brep.hpp"
#include "brep/mesh.hpp"

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
  // Don't require validation layers (often missing); keep startup reliable.
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

  // Build demo body and tessellate.
  {
    using namespace brep;
    static Model model;
    Body* body = make_box(model, BoxSpec{
        .min = Point3d{0, 0, 0},
        .max = Point3d{2, 1, 3},
        .name = "demo_box",
    });
    vulkan_window_->set_meshes(tessellate_body(*body), extract_edges(*body));
    vulkan_window_->camera().target = Point3d{1.0, 0.5, 1.5};
  }

  QWidget* container = QWidget::createWindowContainer(vulkan_window_, this);
  container->setFocusPolicy(Qt::StrongFocus);
  container->setMouseTracking(true);
  // Embedded QVulkanWindow often does not receive mouse events on Windows;
  // also listen on the container so orbit/pan/zoom always work.
  container->installEventFilter(vulkan_window_);
  container->setFocus();
  setCentralWidget(container);
  statusBar()->showMessage(
      QStringLiteral(
          "Left-drag: rotate | Right/Middle-drag: pan | Wheel: zoom | Arrows: rotate"));
}

}  // namespace brep::viewer
