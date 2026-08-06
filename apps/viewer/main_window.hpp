#pragma once

#include "document.hpp"
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
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void on_new_document();
  void on_export_dwg_dxf();

 private:
  void setup_menus();
  void place_view_cube();
  void apply_wheel_zoom(int dy);
  void refresh_window_title();
  void rebind_view_cube_camera();
  [[nodiscard]] QString wood_albedo_path() const;

  DocumentSession document_;
  ecs::World world_;
  std::unique_ptr<QVulkanInstance> vulkan_instance_;
  VulkanWindow* vulkan_window_{nullptr};
  QWidget* viewport_container_{nullptr};
  ViewCubeWidget* view_cube_{nullptr};
};

}  // namespace brep::viewer
