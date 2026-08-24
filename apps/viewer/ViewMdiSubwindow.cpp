#include "ViewMdiSubwindow.h"

#include <QEvent>
#include <QMdiArea>
#include <QResizeEvent>
#include <QTimer>
#include <QWindowStateChangeEvent>

namespace brep::viewer
{

ViewMdiSubWindow::ViewMdiSubWindow(QWidget* parent) : QMdiSubWindow(parent)
{
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint |
                 Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
}

void ViewMdiSubWindow::set_view_caption(char standard_view, int serial)
{
  m_standardView = standard_view;
  m_viewSerial = serial;
}

void ViewMdiSubWindow::ensure_mdi_hooked()
{
  if (auto* mdi = mdiArea())
{
    if (mdi->viewport())
{
      mdi->viewport()->removeEventFilter(this);
      mdi->viewport()->installEventFilter(this);
    }
  }
}

void ViewMdiSubWindow::remember_normal_geometry()
{
  if (!m_rememberGeom || m_filled || isMinimized() || isMaximized()) return;
  m_normalGeometry = geometry();
}

void ViewMdiSubWindow::apply_fill_geometry()
{
  auto* mdi = mdiArea();
  if (!mdi || !mdi->viewport()) return;
  m_rememberGeom = false;
  setGeometry(mdi->viewport()->rect());
  m_rememberGeom = true;
}

void ViewMdiSubWindow::fill_workspace()
{
  ensure_mdi_hooked();
  if (!m_filled)
  {
    remember_normal_geometry();
    m_filled = true;
  }
  // Stay in Normal state so the title-bar buttons remain visible.
  if (isMaximized() || isMinimized())
  {
    showNormal();
  }
  apply_fill_geometry();
}

void ViewMdiSubWindow::clear_fill()
{
  m_filled = false;
  m_wasFilledBeforeMinimize = false;
}

void ViewMdiSubWindow::changeEvent(QEvent* event)
{
  QMdiSubWindow::changeEvent(event);
  if (event->type() != QEvent::WindowStateChange) return;

  const auto* se = static_cast<QWindowStateChangeEvent*>(event);
  const auto old_state = se->oldState();
  const auto now = windowState();

  // Minimized: remember fill so restore can re-expand.
  if (now & Qt::WindowMinimized)
  {
    m_wasFilledBeforeMinimize = m_filled || (old_state & Qt::WindowMaximized);
    return;
  }

  // Restored from minimize.
  if ((old_state & Qt::WindowMinimized) && !(now & Qt::WindowMinimized))
  {
    if (m_wasFilledBeforeMinimize)
  {
      QTimer::singleShot(0, this, [this] { fill_workspace(); });
    }
    return;
  }

  // Intercept real maximize: Qt would merge buttons into the main menu bar.
  if (now & Qt::WindowMaximized)
  {
    const bool restoring = m_filled;
    // Drop Maximized so our frame (min/max/close) stays on the subwindow.
    setWindowState((now | Qt::WindowActive) & ~Qt::WindowMaximized);
    QTimer::singleShot(0, this, [this, restoring] {
      if (restoring)
    {
        m_filled = false;
        if (m_normalGeometry.isValid())
        {
          m_rememberGeom = false;
          setGeometry(m_normalGeometry);
          m_rememberGeom = true;
        }
        else
        {
          resize(720, 480);
        }
      }
      else
      {
        fill_workspace();
      }
      // Refresh maximize/restore glyph if the style polls isMaximized().
      update();
    });
  }
}

void ViewMdiSubWindow::resizeEvent(QResizeEvent* event)
{
  QMdiSubWindow::resizeEvent(event);
  remember_normal_geometry();
}

bool ViewMdiSubWindow::eventFilter(QObject* watched, QEvent* event)
{
  if (event->type() == QEvent::Resize && m_filled && !isMinimized())
{
    if (auto* mdi = mdiArea(); mdi && watched == mdi->viewport())
{
      apply_fill_geometry();
    }
  }
  return QMdiSubWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
