#include "MainWindow.h"

#include "api/Core.h"
#include "commands/CommandRegistry.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"
#include "i18n/LanguageManager.h"

#include <QApplication>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QVersionNumber>
#include <QVulkanInstance>

#include <stdexcept>

namespace brep::viewer
{

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_commandManager(m_commands)
{
  resize(1100, 720);

  commands::register_builtin_commands(m_commands);
  QSettings settings;
  m_snapSettings = commands::load_snap_settings(settings);

  m_vulkanInstance = std::make_unique<QVulkanInstance>();
  m_vulkanInstance->setApiVersion(QVersionNumber(1, 2, 0));
  if (!m_vulkanInstance->create())
  {
    m_vulkanInstance->setApiVersion(QVersionNumber(1, 0, 0));
    if (!m_vulkanInstance->create())
    {
      throw std::runtime_error("Failed to create QVulkanInstance");
    }
  }

  m_document.new_blank_document(m_world);
  BREP_INFO("ECS scene ready: blank Document + Part + camera");

  m_mdiArea = new QMdiArea(this);
  m_mdiArea->setViewMode(QMdiArea::SubWindowView);
  m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_mdiArea->setTabsClosable(true);
  setCentralWidget(m_mdiArea);
  setMouseTracking(true);
  m_mdiArea->installEventFilter(this);
  if (m_mdiArea->viewport())
  {
    m_mdiArea->viewport()->installEventFilter(this);
  }

  connect(m_mdiArea, &QMdiArea::subWindowActivated, this,
          &MainWindow::on_sub_window_activated);

  create_view_window('h');

  m_viewCube = new ViewCubeWidget(this);
  m_viewCube->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                             Qt::WindowDoesNotAcceptFocus);
  m_viewCube->setAttribute(Qt::WA_ShowWithoutActivating);
  rebind_view_cube_camera();
  m_viewCube->set_redraw_callback([this] {
    if (auto* vw = active_vulkan_window()) vw->requestUpdate();
    if (auto* container = active_viewport_container())
    {
      container->setFocus(Qt::OtherFocusReason);
    }
  });
  place_view_cube();
  m_viewCube->hide();

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
          [this](const QString& tag)
  {
            retranslate_ui();
            statusBar()->showMessage(tr("Language: %1").arg(tag), 3000);
          });

  qApp->installEventFilter(this);

  statusBar()->showMessage(QStringLiteral(
      "XCAD | 左键选择/框选 | Ctrl+追加 | 右键菜单 | "
      "中键平移 | Ctrl+中键旋转 | 双击中键缩放到全部 | ESC 取消工具"));
}

MainWindow::~MainWindow()
{
  BREP_INFO("MainWindow::~MainWindow begin");
  // Must remove before QObject teardown; otherwise quit-time events can call
  // into a destroyed MainWindow (ACCESS_VIOLATION / 0xC0000005).
  if (qApp) qApp->removeEventFilter(this);
  if (m_toolCursorOverridden)
  {
    QApplication::restoreOverrideCursor();
    m_toolCursorOverridden = false;
  }

  // QObject children (MDI / QVulkanWindow) are destroyed in ~QObject, AFTER
  // member unique_ptrs. QVulkanInstance must outlive every QVulkanWindow, so
  // destroy Vulkan views here before m_vulkanInstance is reset/destroyed.
  m_suppressEnsureView = true;
  if (m_viewCube)
  {
    BREP_INFO("MainWindow::~MainWindow: destroy ViewCube");
    m_viewCube->set_redraw_callback({});
    m_viewCube->set_camera(nullptr);
    delete m_viewCube;
    m_viewCube = nullptr;
  }
  if (m_mdiArea)
  {
    BREP_INFO("MainWindow::~MainWindow: destroy {} MDI subwindow(s)",
              m_mdiArea->subWindowList().size());
    const auto subs = m_mdiArea->subWindowList();
    for (auto* sub : subs)
    {
      if (!sub) continue;
      BREP_INFO("MainWindow::~MainWindow: delete subwindow '{}'",
                sub->windowTitle().toStdString());
      m_mdiArea->removeSubWindow(sub);
      delete sub;
    }
    m_viewWindows.clear();
    BREP_INFO("MainWindow::~MainWindow: delete QMdiArea");
    setCentralWidget(nullptr);
    delete m_mdiArea;
    m_mdiArea = nullptr;
  }

  BREP_INFO("MainWindow::~MainWindow: reset QVulkanInstance");
  m_vulkanInstance.reset();
  BREP_INFO("MainWindow::~MainWindow end");
}

}  // namespace brep::viewer
