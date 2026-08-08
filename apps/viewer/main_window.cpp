#include "main_window.hpp"

#include "brep/log.hpp"
#include "commands/command_registry.hpp"
#include "ecs/components.hpp"
#include "ecs/systems.hpp"
#include "i18n/language_manager.hpp"

#include <QApplication>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QStatusBar>
#include <QTimer>
#include <QVersionNumber>
#include <QVulkanInstance>

#include <stdexcept>

namespace brep::viewer {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), command_manager_(commands_) {
  resize(1100, 720);

  commands::register_builtin_commands(commands_);

  vulkan_instance_ = std::make_unique<QVulkanInstance>();
  vulkan_instance_->setApiVersion(QVersionNumber(1, 2, 0));
  if (!vulkan_instance_->create()) {
    vulkan_instance_->setApiVersion(QVersionNumber(1, 0, 0));
    if (!vulkan_instance_->create()) {
      throw std::runtime_error("Failed to create QVulkanInstance");
    }
  }

  document_.new_blank_document(world_);
  BREP_INFO("ECS scene ready: blank Document + Part + camera");

  mdi_area_ = new QMdiArea(this);
  mdi_area_->setViewMode(QMdiArea::SubWindowView);
  mdi_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mdi_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mdi_area_->setTabsClosable(true);
  setCentralWidget(mdi_area_);
  setMouseTracking(true);
  mdi_area_->installEventFilter(this);
  if (mdi_area_->viewport()) {
    mdi_area_->viewport()->installEventFilter(this);
  }

  connect(mdi_area_, &QMdiArea::subWindowActivated, this,
          &MainWindow::on_sub_window_activated);

  create_view_window('h');

  view_cube_ = new ViewCubeWidget(this);
  view_cube_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                             Qt::WindowDoesNotAcceptFocus);
  view_cube_->setAttribute(Qt::WA_ShowWithoutActivating);
  rebind_view_cube_camera();
  view_cube_->set_redraw_callback([this] {
    if (auto* vw = active_vulkan_window()) vw->requestUpdate();
    if (auto* container = active_viewport_container()) {
      container->setFocus(Qt::OtherFocusReason);
    }
  });
  place_view_cube();
  view_cube_->hide();

  setup_cursor_tip();
  setup_menus();
  setup_toolbar();
  setup_view_toolbar();
  setup_property_dock();
  retranslate_ui();
  refresh_window_title();
  refresh_edit_actions();
  update_property_panel(entt::null);

  connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this,
          [this](const QString& tag) {
            retranslate_ui();
            statusBar()->showMessage(tr("Language: %1").arg(tag), 3000);
          });

  qApp->installEventFilter(this);

  statusBar()->showMessage(QStringLiteral(
      "XCAD | 左键选择/框选 | Ctrl+追加 | 右键菜单 | "
      "中键平移 | Ctrl+中键旋转 | 双击中键缩放到全部 | ESC 取消工具"));
}

MainWindow::~MainWindow() {
  BREP_INFO("MainWindow::~MainWindow begin");
  // Must remove before QObject teardown; otherwise quit-time events can call
  // into a destroyed MainWindow (ACCESS_VIOLATION / 0xC0000005).
  if (qApp) qApp->removeEventFilter(this);
  if (tool_cursor_overridden_) {
    QApplication::restoreOverrideCursor();
    tool_cursor_overridden_ = false;
  }

  // QObject children (MDI / QVulkanWindow) are destroyed in ~QObject, AFTER
  // member unique_ptrs. QVulkanInstance must outlive every QVulkanWindow, so
  // destroy Vulkan views here before vulkan_instance_ is reset/destroyed.
  suppress_ensure_view_ = true;
  if (view_cube_) {
    BREP_INFO("MainWindow::~MainWindow: destroy ViewCube");
    view_cube_->set_redraw_callback({});
    view_cube_->set_camera(nullptr);
    delete view_cube_;
    view_cube_ = nullptr;
  }
  if (mdi_area_) {
    BREP_INFO("MainWindow::~MainWindow: destroy {} MDI subwindow(s)",
              mdi_area_->subWindowList().size());
    const auto subs = mdi_area_->subWindowList();
    for (auto* sub : subs) {
      if (!sub) continue;
      BREP_INFO("MainWindow::~MainWindow: delete subwindow '{}'",
                sub->windowTitle().toStdString());
      mdi_area_->removeSubWindow(sub);
      delete sub;
    }
    view_windows_.clear();
    BREP_INFO("MainWindow::~MainWindow: delete QMdiArea");
    setCentralWidget(nullptr);
    delete mdi_area_;
    mdi_area_ = nullptr;
  }

  BREP_INFO("MainWindow::~MainWindow: reset QVulkanInstance");
  vulkan_instance_.reset();
  BREP_INFO("MainWindow::~MainWindow end");
}

}  // namespace brep::viewer
