#pragma once

#include "vulkan_window.hpp"

#include <QMainWindow>

namespace brep::viewer {

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private:
  VulkanWindow* vulkan_window_{nullptr};
};

}  // namespace brep::viewer
