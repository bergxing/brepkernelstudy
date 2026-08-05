#include "main_window.hpp"

#include "brep/brep.hpp"
#include "brep/mesh.hpp"

#include <QStatusBar>
#include <QVulkanInstance>
#include <QWidget>

#include <stdexcept>

namespace brep::viewer {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("B-Rep Kernel Viewer"));
  resize(1100, 720);

  auto* inst = new QVulkanInstance(this);
  inst->setLayers({QByteArrayLiteral("VK_LAYER_KHRONOS_validation")});
  if (!inst->create()) {
    // Validation layer may be absent; retry without it.
    inst->setLayers({});
    if (!inst->create()) {
      throw std::runtime_error("Failed to create QVulkanInstance");
    }
  }

  vulkan_window_ = new VulkanWindow();
  vulkan_window_->setVulkanInstance(inst);

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
  setCentralWidget(container);
  statusBar()->showMessage(
      QStringLiteral("Left-drag: orbit | Wheel: zoom | Vulkan via QVulkanWindow"));
}

}  // namespace brep::viewer
