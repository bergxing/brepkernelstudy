#include "main_window.hpp"

#include "assets/asset_catalog.hpp"

#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSignalBlocker>

namespace brep::viewer {

QString MainWindow::wood_albedo_path() const {
  // QImage (Vulkan texture upload) accepts Qt resource URLs.
  if (!AssetCatalog::exists(QStringLiteral("wood.png"))) return {};
  return AssetCatalog::url(QStringLiteral("wood.png"));
}

void MainWindow::apply_standard_view(char face) {
  auto* vw = active_vulkan_window();
  if (!vw) return;
  vw->camera().set_standard_view(face);
  if (act_ortho_) {
    const QSignalBlocker block(act_ortho_);
    act_ortho_->setChecked(vw->camera().ortho);
  }
  if (view_cube_) view_cube_->update();
  vw->requestUpdate();
}

void MainWindow::rebind_view_cube_camera() {
  auto* vw = active_vulkan_window();
  if (view_cube_) view_cube_->set_camera(vw ? &vw->camera() : nullptr);
  if (act_ortho_ && vw) {
    const QSignalBlocker block(act_ortho_);
    act_ortho_->setChecked(vw->camera().ortho);
  }
}

void MainWindow::place_view_cube() {
  if (!view_cube_) return;

  QWidget* container = active_viewport_container();
  QMdiSubWindow* sub = mdi_area_ ? mdi_area_->activeSubWindow() : nullptr;
  if (!container || !sub || sub->isMinimized() || !container->isVisible() ||
      isMinimized()) {
    view_cube_->hide();
    return;
  }

  constexpr int margin = 10;
  // Anchor to the active viewport's top-right corner (client area).
  const QPoint global = container->mapToGlobal(
      QPoint(container->width() - view_cube_->width() - margin, margin));
  view_cube_->move(global);
  if (!view_cube_->isVisible()) view_cube_->show();
}

}  // namespace brep::viewer
