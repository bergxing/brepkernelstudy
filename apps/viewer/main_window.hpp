#pragma once

#include "vulkan_window.hpp"

#include <QMainWindow>
#include <QVulkanInstance>

#include <memory>

namespace brep::viewer {

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private:
  std::unique_ptr<QVulkanInstance> vulkan_instance_;
  VulkanWindow* vulkan_window_{nullptr};
};

}  // namespace brep::viewer
