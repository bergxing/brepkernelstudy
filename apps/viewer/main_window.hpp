#pragma once

#include "ecs/world.hpp"
#include "view_cube.hpp"
#include "vulkan_window.hpp"

#include <QMainWindow>
#include <QVulkanInstance>

#include <memory>

namespace brep::viewer {

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void moveEvent(QMoveEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  void place_view_cube();

  ecs::World world_;
  std::unique_ptr<QVulkanInstance> vulkan_instance_;
  VulkanWindow* vulkan_window_{nullptr};
  QWidget* viewport_container_{nullptr};
  ViewCubeWidget* view_cube_{nullptr};
};

}  // namespace brep::viewer
