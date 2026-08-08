#pragma once

#include "commands/command_manager.hpp"
#include "commands/command_registry.hpp"
#include "document.hpp"
#include "ecs/world.hpp"
#include "property_panel.hpp"
#include "view_cube.hpp"
#include "vulkan_window.hpp"

#include <QHash>
#include <QMainWindow>
#include <QVulkanInstance>

#include <memory>

class QAction;
class QDockWidget;
class QMdiArea;
class QMdiSubWindow;
class QToolBar;

namespace brep::viewer {

class MainWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

  /// Load a `.xl` document into the workspace and show its content.
  [[nodiscard]] bool open_document(const QString& path);

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
  void on_sub_window_activated(QMdiSubWindow* sub);
  void on_new_view();
  void on_quad_views();
  void on_tile_views();
  void on_cascade_views();
  void on_close_active_view();

 private:
  VulkanWindow* create_view_window(const QString& title, char standard_view,
                                   bool fill_workspace = true);
  void clear_view_fill_states();
  [[nodiscard]] VulkanWindow* active_vulkan_window() const;
  [[nodiscard]] QWidget* active_viewport_container() const;
  [[nodiscard]] VulkanWindow* vulkan_window_for_sub(QMdiSubWindow* sub) const;
  [[nodiscard]] VulkanWindow* vulkan_window_at_global(const QPoint& global) const;
  void wire_vulkan_window(VulkanWindow* window);
  void request_all_views_update();
  void ensure_minimum_view();
  void setup_menus();
  void setup_window_menu();
  void setup_toolbar();
  void setup_view_toolbar();
  void place_view_cube();
  [[nodiscard]] bool is_view_layout_object(const QObject* watched) const;
  void apply_wheel_zoom(VulkanWindow* window, int dy);
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
  QMdiArea* mdi_area_{nullptr};
  QHash<QMdiSubWindow*, VulkanWindow*> view_windows_;
  int view_serial_{0};
  bool suppress_ensure_view_{false};
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
