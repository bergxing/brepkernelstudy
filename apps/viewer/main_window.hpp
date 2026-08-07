#pragma once

#include "commands/command_registry.hpp"
#include "document.hpp"
#include "ecs/world.hpp"
#include "view_cube.hpp"
#include "vulkan_window.hpp"

#include <QMainWindow>
#include <QVulkanInstance>

#include <memory>

class QAction;
class QToolBar;

namespace brep::viewer {

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 protected:
  void showEvent(QShowEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void moveEvent(QMoveEvent* event) override;
  void changeEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private slots:
  void on_run_command();

 private:
  void setup_menus();
  void setup_toolbar();
  void place_view_cube();
  void apply_wheel_zoom(int dy);
  void refresh_window_title();
  void rebind_view_cube_camera();
  void bind_action(QAction* action, const char* command_id);
  commands::CommandResult run_command(std::string_view command_id);
  [[nodiscard]] commands::CommandContext make_command_context();
  [[nodiscard]] QString wood_albedo_path() const;

  DocumentSession document_;
  ecs::World world_;
  commands::CommandRegistry commands_;
  std::unique_ptr<QVulkanInstance> vulkan_instance_;
  VulkanWindow* vulkan_window_{nullptr};
  QWidget* viewport_container_{nullptr};
  ViewCubeWidget* view_cube_{nullptr};
  QToolBar* toolbar_{nullptr};
};

}  // namespace brep::viewer
