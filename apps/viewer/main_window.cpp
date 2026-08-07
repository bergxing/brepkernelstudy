#include "main_window.hpp"

#include "brep/log.hpp"
#include "ecs/components.hpp"
#include "io/dxf_export.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStatusBar>
#include <QToolBar>
#include <QVersionNumber>
#include <QVulkanInstance>
#include <QWheelEvent>
#include <QWidget>

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace brep::viewer {
namespace {

int wheel_delta_y(const QWheelEvent* event) {
  if (event->angleDelta().y() != 0) return event->angleDelta().y();
  if (event->pixelDelta().y() != 0) return event->pixelDelta().y();
  return 0;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  resize(1100, 720);

  vulkan_instance_ = std::make_unique<QVulkanInstance>();
  vulkan_instance_->setApiVersion(QVersionNumber(1, 2, 0));
  if (!vulkan_instance_->create()) {
    vulkan_instance_->setApiVersion(QVersionNumber(1, 0, 0));
    if (!vulkan_instance_->create()) {
      throw std::runtime_error("Failed to create QVulkanInstance");
    }
  }

  vulkan_window_ = new VulkanWindow();
  vulkan_window_->setVulkanInstance(vulkan_instance_.get());
  vulkan_window_->setSampleCount(1);
  vulkan_window_->set_world(&world_);

  // Start with a blank document (no bodies).
  document_.new_blank_document(world_);
  BREP_INFO("ECS scene ready: blank Document + Part + camera");

  viewport_container_ = QWidget::createWindowContainer(vulkan_window_, this);
  viewport_container_->setFocusPolicy(Qt::StrongFocus);
  viewport_container_->setMouseTracking(true);
  viewport_container_->installEventFilter(vulkan_window_);
  viewport_container_->setFocus();
  setCentralWidget(viewport_container_);

  view_cube_ = new ViewCubeWidget(this);
  view_cube_->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                             Qt::WindowDoesNotAcceptFocus);
  view_cube_->setAttribute(Qt::WA_ShowWithoutActivating);
  rebind_view_cube_camera();
  view_cube_->set_redraw_callback([this] {
    if (vulkan_window_) vulkan_window_->requestUpdate();
    if (viewport_container_) {
      viewport_container_->setFocus(Qt::OtherFocusReason);
    }
  });
  place_view_cube();
  view_cube_->hide();  // shown with MainWindow after splash

  setup_menus();
  setup_toolbar();
  refresh_window_title();

  qApp->installEventFilter(this);

  statusBar()->showMessage(
      QStringLiteral("XCAD | 新建空白文档 | 导出 DXF | ViewCube / 滚轮缩放"));
}

QString MainWindow::wood_albedo_path() const {
  QString wood_path = QStringLiteral(BREP_VIEWER_ASSETS_DIR "/wood.png");
  if (!QFileInfo::exists(wood_path)) {
    wood_path = QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("assets/wood.png"));
  }
  return wood_path;
}

void MainWindow::setup_menus() {
  auto* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

  auto* act_new = file_menu->addAction(QStringLiteral("新建(&N)"));
  act_new->setShortcut(QKeySequence::New);
  act_new->setToolTip(QStringLiteral("新建空白文档"));
  connect(act_new, &QAction::triggered, this, &MainWindow::on_new_document);

  auto* act_export =
      file_menu->addAction(QStringLiteral("导出 DWG/DXF(&E)…"));
  act_export->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
  connect(act_export, &QAction::triggered, this,
          &MainWindow::on_export_dwg_dxf);
}

void MainWindow::setup_toolbar() {
  toolbar_ = addToolBar(QStringLiteral("主工具栏"));
  toolbar_->setMovable(false);
  toolbar_->setIconSize(QSize(20, 20));

  auto* act_new = toolbar_->addAction(QStringLiteral("新建"));
  act_new->setToolTip(QStringLiteral("新建空白文档 (Ctrl+N)"));
  act_new->setShortcut(QKeySequence::New);
  connect(act_new, &QAction::triggered, this, &MainWindow::on_new_document);

  auto* act_export = toolbar_->addAction(QStringLiteral("导出 DXF"));
  act_export->setToolTip(QStringLiteral("导出 DWG/DXF 线框 (Ctrl+E)"));
  connect(act_export, &QAction::triggered, this,
          &MainWindow::on_export_dwg_dxf);
}

void MainWindow::refresh_window_title() {
  setWindowTitle(document_.window_title());
}

void MainWindow::rebind_view_cube_camera() {
  if (view_cube_) view_cube_->set_camera(world_.main_camera());
}

void MainWindow::on_new_document() {
  document_.new_blank_document(world_);
  rebind_view_cube_camera();
  refresh_window_title();
  if (vulkan_window_) vulkan_window_->requestUpdate();
  statusBar()->showMessage(QStringLiteral("已新建空白文档"), 4000);
  BREP_INFO("document: new blank Document/Part");
}

void MainWindow::on_export_dwg_dxf() {
  QString start = document_.path();
  if (start.isEmpty()) {
    start = QDir::homePath() + QStringLiteral("/untitled.dxf");
  }

  QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出 DWG/DXF"), start,
      QStringLiteral("CAD Drawing (*.dxf);;All Files (*)"));
  if (path.isEmpty()) return;

  if (!path.endsWith(QStringLiteral(".dxf"), Qt::CaseInsensitive)) {
    path += QStringLiteral(".dxf");
  }

  std::vector<io::DxfSegment> segments;
  auto view =
      world_.registry().view<ecs::MeshComponent, ecs::Transform>();
  for (auto entity : view) {
    const auto& mesh = view.get<ecs::MeshComponent>(entity);
    const auto& xform = view.get<ecs::Transform>(entity);
    io::append_edge_segments(mesh.edges, xform.position, segments);
  }

  const std::filesystem::path fs_path = path.toStdString();
  if (!io::write_edges_dxf(fs_path, segments)) {
    const QString err = QString::fromStdString(io::last_dxf_error());
    QMessageBox::critical(this, QStringLiteral("导出失败"), err);
    BREP_ERROR("dxf export failed: {}", io::last_dxf_error());
    return;
  }

  document_.set_export_path(path);
  refresh_window_title();
  statusBar()->showMessage(
      QStringLiteral("已导出 DXF 线框（%1 段）: %2")
          .arg(segments.size())
          .arg(path),
      6000);
  BREP_INFO("exported DXF {} segments -> {}", segments.size(),
            path.toStdString());
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  if (view_cube_) {
    view_cube_->show();
    place_view_cube();
  }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  place_view_cube();
}

void MainWindow::moveEvent(QMoveEvent* event) {
  QMainWindow::moveEvent(event);
  place_view_cube();
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
  if (!view_cube_) return;
  if (event->type() == QEvent::WindowStateChange) {
    if (isMinimized()) {
      view_cube_->hide();
    } else {
      view_cube_->show();
      place_view_cube();
    }
  }
}

void MainWindow::place_view_cube() {
  if (!view_cube_ || !viewport_container_) return;
  constexpr int margin = 10;
  const QPoint global = viewport_container_->mapToGlobal(
      QPoint(viewport_container_->width() - view_cube_->width() - margin,
             margin));
  view_cube_->move(global);
}

void MainWindow::apply_wheel_zoom(int dy) {
  if (dy == 0) return;

  Camera* cam = world_.main_camera();
  if (!cam) {
    BREP_ERROR("wheel zoom: main camera missing");
    return;
  }

  const float dist0 = cam->distance;
  const float half0 = cam->ortho_half_h;
  const bool ortho0 = cam->ortho;
  cam->zoom(dy > 0 ? 1.0f : -1.0f);

  BREP_INFO(
      "wheel zoom delta={} ortho={} dist {:.3f}->{:.3f} orthoHalf {:.3f}->{:.3f}",
      dy, ortho0, dist0, cam->distance, half0, cam->ortho_half_h);

  statusBar()->showMessage(
      QStringLiteral("Zoom | dist=%1  orthoHalf=%2  mode=%3")
          .arg(cam->distance, 0, 'f', 2)
          .arg(cam->ortho_half_h, 0, 'f', 2)
          .arg(cam->ortho ? QStringLiteral("ortho")
                          : QStringLiteral("persp")));

  if (vulkan_window_) vulkan_window_->requestUpdate();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Wheel && vulkan_window_ && viewport_container_) {
    auto* we = static_cast<QWheelEvent*>(event);
    const QPoint global = we->globalPosition().toPoint();

    const QRect viewport_global(
        viewport_container_->mapToGlobal(QPoint(0, 0)),
        viewport_container_->size());
    if (!viewport_global.contains(global)) {
      return QMainWindow::eventFilter(watched, event);
    }

    if (view_cube_ && view_cube_->isVisible()) {
      const QRect cube_global(view_cube_->pos(), view_cube_->size());
      if (cube_global.contains(global)) {
        return QMainWindow::eventFilter(watched, event);
      }
    }

    const int dy = wheel_delta_y(we);
    if (dy != 0) {
      apply_wheel_zoom(dy);
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
