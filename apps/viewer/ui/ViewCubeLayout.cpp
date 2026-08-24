#include "MainWindow.h"

#include "assets/AssetCatalog.h"

#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSignalBlocker>

namespace brep::viewer
{

QString MainWindow::wood_albedo_path() const
{
  // QImage (Vulkan texture upload) accepts Qt resource URLs.
  if (!AssetCatalog::exists(QStringLiteral("wood.png"))) return {};
  return AssetCatalog::url(QStringLiteral("wood.png"));
}

void MainWindow::apply_standard_view(char face)
{
  auto* vw = active_vulkan_window();
  if (!vw) return;
  vw->camera().set_standard_view(face);
  if (m_actOrtho)
  {
    const QSignalBlocker block(m_actOrtho);
    m_actOrtho->setChecked(vw->camera().ortho);
  }
  if (m_viewCube) m_viewCube->update();
  vw->requestUpdate();
}

void MainWindow::rebind_view_cube_camera()
{
  auto* vw = active_vulkan_window();
  if (m_viewCube) m_viewCube->set_camera(vw ? &vw->camera() : nullptr);
  if (m_actOrtho && vw)
  {
    const QSignalBlocker block(m_actOrtho);
    m_actOrtho->setChecked(vw->camera().ortho);
  }
}

void MainWindow::place_view_cube()
{
  if (!m_viewCube) return;

  QWidget* container = active_viewport_container();
  QMdiSubWindow* sub = m_mdiArea ? m_mdiArea->activeSubWindow() : nullptr;
  if (!container || !sub || sub->isMinimized() || !container->isVisible() ||
      isMinimized())
  {
    m_viewCube->hide();
    return;
  }

  constexpr int margin = 10;
  // Anchor to the active viewport's top-right corner (client area).
  const QPoint global = container->mapToGlobal(
      QPoint(container->width() - m_viewCube->width() - margin, margin));
  m_viewCube->move(global);
  if (!m_viewCube->isVisible()) m_viewCube->show();
}

}  // namespace brep::viewer
