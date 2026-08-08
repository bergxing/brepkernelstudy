#pragma once

#include "commands/command_manager.hpp"
#include "commands/command_registry.hpp"
#include "document.hpp"
#include "ecs/world.hpp"
#include "property_panel.hpp"
#include "view_cube.hpp"
#include "vulkan_window.hpp"

#include <QMainWindow>
#include <QVulkanInstance>

#include <memory>

class QAction;
class QDockWidget;
class QToolBar;

namespace brep::viewer {

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void showEvent(QShowEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void moveEvent(QMoveEvent* event) override;
  void changeEvent(QEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private slots:
  void on_run_command();
  void on_command_palette();

 private:
  void setup_menus();
  void setup_toolbar();
  void setup_view_toolbar();
  void place_view_cube();
  void apply_wheel_zoom(int dy);
  void apply_standard_view(char face);
  void refresh_window_title();
  void refresh_edit_actions();
  void rebind_view_cube_camera();
  void sync_tool_ui();
  void setup_property_dock();
  void update_property_panel(entt::entity entity);
  void bind_action(QAction* action, const char* command_id);
  commands::CommandResult run_command(std::string_view command_id);
  [[nodiscard]] commands::CommandContext make_command_context();
  [[nodiscard]] QString wood_albedo_path() const;
  [[nodiscard]] QString view_icon_path(const QString& filename) const;
  bool handle_tool_mouse(QEvent* event);

  DocumentSession document_;
  ecs::World world_;
  commands::CommandRegistry commands_;
  commands::CommandManager command_manager_;
  std::unique_ptr<QVulkanInstance> vulkan_instance_;
  VulkanWindow* vulkan_window_{nullptr};
  QWidget* viewport_container_{nullptr};
  ViewCubeWidget* view_cube_{nullptr};
  QToolBar* toolbar_{nullptr};
  QToolBar* view_toolbar_{nullptr};
  QDockWidget* property_dock_{nullptr};
  PropertyPanel* property_panel_{nullptr};
  QAction* act_undo_{nullptr};
  QAction* act_redo_{nullptr};
  QAction* act_ortho_{nullptr};
  bool tool_cursor_overridden_{false};
};

}  // namespace brep::viewer
