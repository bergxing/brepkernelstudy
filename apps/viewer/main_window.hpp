#pragma once

#include "adapter/scene_adapter.hpp"
#include "commands/command_manager.hpp"
#include "commands/command_registry.hpp"
#include "commands/snap/snap_settings.hpp"
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
class QActionGroup;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QMdiArea;
class QMdiSubWindow;
class QMenu;
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
  void closeEvent(QCloseEvent* event) override;
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
  VulkanWindow* create_view_window(char standard_view,
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
  void setup_language_menu(QMenu* tools_menu);
  void setup_toolbar();
  void setup_view_toolbar();
  void retranslate_ui();
  void sync_language_menu_checks();
  void refresh_view_titles();
  [[nodiscard]] QString title_for_standard_view(char face) const;
  [[nodiscard]] QString format_view_title(char standard_view, int serial) const;
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
  /// Returns false if the user cancelled closing (keep the window open).
  [[nodiscard]] bool confirm_close_or_save();
  [[nodiscard]] commands::CommandContext make_command_context();
  [[nodiscard]] QString wood_albedo_path() const;
  bool handle_tool_mouse(QEvent* event);
  void show_viewport_context_menu(VulkanWindow* window, float x, float y);
  void setup_cursor_tip();
  void hide_cursor_tip();
  void refresh_cursor_tip();
  void update_cursor_tip_at_global(const QPoint& global);
  [[nodiscard]] QString resolve_cursor_tip_text(VulkanWindow* window, float x,
                                                float y);
  [[nodiscard]] bool map_global_to_viewport(const QPoint& global,
                                            VulkanWindow*& out_window,
                                            float& out_x, float& out_y) const;

  DocumentSession document_;
  ecs::World world_;
  adapter::SceneAdapter scene_adapter_;
  commands::CommandRegistry commands_;
  commands::CommandManager command_manager_;
  commands::SnapSettings snap_settings_;
  commands::SnapSession snap_session_;
  std::unique_ptr<QVulkanInstance> vulkan_instance_;
  QMdiArea* mdi_area_{nullptr};
  QHash<QMdiSubWindow*, VulkanWindow*> view_windows_;
  int view_serial_{0};
  bool suppress_ensure_view_{false};
  ViewCubeWidget* view_cube_{nullptr};
  QLabel* cursor_tip_{nullptr};
  QToolBar* toolbar_{nullptr};
  QToolBar* view_toolbar_{nullptr};
  QDockWidget* property_dock_{nullptr};
  PropertyPanel* property_panel_{nullptr};
  QAction* act_undo_{nullptr};
  QAction* act_redo_{nullptr};
  QAction* act_ortho_{nullptr};
  QActionGroup* lang_action_group_{nullptr};
  QAction* act_lang_system_{nullptr};
  QAction* act_lang_zh_{nullptr};
  QAction* act_lang_en_{nullptr};
  bool tool_cursor_overridden_{false};
};

}  // namespace brep::viewer
