#include "view_mdi_subwindow.hpp"

#include <QEvent>
#include <QMdiArea>
#include <QResizeEvent>
#include <QTimer>
#include <QWindowStateChangeEvent>

namespace brep::viewer {

ViewMdiSubWindow::ViewMdiSubWindow(QWidget* parent) : QMdiSubWindow(parent) {
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint |
                 Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
}

void ViewMdiSubWindow::set_view_caption(char standard_view, int serial) {
  standard_view_ = standard_view;
  view_serial_ = serial;
}

void ViewMdiSubWindow::ensure_mdi_hooked() {
  if (auto* mdi = mdiArea()) {
    if (mdi->viewport()) {
      mdi->viewport()->removeEventFilter(this);
      mdi->viewport()->installEventFilter(this);
    }
  }
}

void ViewMdiSubWindow::remember_normal_geometry() {
  if (!remember_geom_ || filled_ || isMinimized() || isMaximized()) return;
  normal_geometry_ = geometry();
}

void ViewMdiSubWindow::apply_fill_geometry() {
  auto* mdi = mdiArea();
  if (!mdi || !mdi->viewport()) return;
  remember_geom_ = false;
  setGeometry(mdi->viewport()->rect());
  remember_geom_ = true;
}

void ViewMdiSubWindow::fill_workspace() {
  ensure_mdi_hooked();
  if (!filled_) {
    remember_normal_geometry();
    filled_ = true;
  }
  // Stay in Normal state so the title-bar buttons remain visible.
  if (isMaximized() || isMinimized()) {
    showNormal();
  }
  apply_fill_geometry();
}

void ViewMdiSubWindow::clear_fill() {
  filled_ = false;
  was_filled_before_minimize_ = false;
}

void ViewMdiSubWindow::changeEvent(QEvent* event) {
  QMdiSubWindow::changeEvent(event);
  if (event->type() != QEvent::WindowStateChange) return;

  const auto* se = static_cast<QWindowStateChangeEvent*>(event);
  const auto old_state = se->oldState();
  const auto now = windowState();

  // Minimized: remember fill so restore can re-expand.
  if (now & Qt::WindowMinimized) {
    was_filled_before_minimize_ = filled_ || (old_state & Qt::WindowMaximized);
    return;
  }

  // Restored from minimize.
  if ((old_state & Qt::WindowMinimized) && !(now & Qt::WindowMinimized)) {
    if (was_filled_before_minimize_) {
      QTimer::singleShot(0, this, [this] { fill_workspace(); });
    }
    return;
  }

  // Intercept real maximize: Qt would merge buttons into the main menu bar.
  if (now & Qt::WindowMaximized) {
    const bool restoring = filled_;
    // Drop Maximized so our frame (min/max/close) stays on the subwindow.
    setWindowState((now | Qt::WindowActive) & ~Qt::WindowMaximized);
    QTimer::singleShot(0, this, [this, restoring] {
      if (restoring) {
        filled_ = false;
        if (normal_geometry_.isValid()) {
          remember_geom_ = false;
          setGeometry(normal_geometry_);
          remember_geom_ = true;
        } else {
          resize(720, 480);
        }
      } else {
        fill_workspace();
      }
      // Refresh maximize/restore glyph if the style polls isMaximized().
      update();
    });
  }
}

void ViewMdiSubWindow::resizeEvent(QResizeEvent* event) {
  QMdiSubWindow::resizeEvent(event);
  remember_normal_geometry();
}

bool ViewMdiSubWindow::eventFilter(QObject* watched, QEvent* event) {
  if (event->type() == QEvent::Resize && filled_ && !isMinimized()) {
    if (auto* mdi = mdiArea(); mdi && watched == mdi->viewport()) {
      apply_fill_geometry();
    }
  }
  return QMdiSubWindow::eventFilter(watched, event);
}

}  // namespace brep::viewer
