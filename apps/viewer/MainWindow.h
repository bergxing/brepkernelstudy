#pragma once

#include "adapter/SceneAdapter.h"
#include "commands/CommandManager.h"
#include "commands/CommandRegistry.h"
#include "commands/snap/SnapSettings.h"
#include "Document.h"
#include "ecs/World.h"
#include "PropertyPanel.h"
#include "ViewCube.h"
#include "VulkanWindow.h"

#include <QHash>
#include <QMainWindow>
#include <QVulkanInstance>

#include <memory>
#include <vector>

class QAction;
class QActionGroup;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QMdiArea;
class QMdiSubWindow;
class QMenu;
class QToolBar;

namespace brep::viewer
{

class MainWindow final : public QMainWindow
{
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
  void keyReleaseEvent(QKeyEvent* event) override;

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
  void show_snap_settings();
  void set_snap_enabled(bool enabled);
  void save_snap_settings();
  void sync_snap_action();
  bool handle_snap_key(QKeyEvent* event, bool pressed);
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

  DocumentSession m_document;
  ecs::World m_world;
  adapter::SceneAdapter m_sceneAdapter;
  commands::CommandRegistry m_commands;
  commands::CommandManager m_commandManager;
  commands::SnapSettings m_snapSettings;
  commands::SnapSession m_snapSession;
  std::unique_ptr<QVulkanInstance> m_vulkanInstance;
  QMdiArea* m_mdiArea{nullptr};
  QHash<QMdiSubWindow*, VulkanWindow*> m_viewWindows;
  int m_viewSerial{0};
  bool m_suppressEnsureView{false};
  ViewCubeWidget* m_viewCube{nullptr};
  QLabel* m_cursorTip{nullptr};
  QToolBar* m_toolbar{nullptr};
  QToolBar* m_viewToolbar{nullptr};
  QDockWidget* m_propertyDock{nullptr};
  PropertyPanel* m_propertyPanel{nullptr};
  QAction* m_actUndo{nullptr};
  QAction* m_actRedo{nullptr};
  QAction* m_actOrtho{nullptr};
  QAction* m_actSnapEnabled{nullptr};
  QActionGroup* m_langActionGroup{nullptr};
  QAction* m_actLangSystem{nullptr};
  QAction* m_actLangZh{nullptr};
  QAction* m_actLangEn{nullptr};
  std::vector<int> m_heldSnapOverrideKeys;
  bool m_toolCursorOverridden{false};
};

}  // namespace brep::viewer
