#include "select_rect_overlay.hpp"

#include <QPainter>
#include <QPaintEvent>

namespace brep::viewer {

SelectRectOverlay::SelectRectOverlay(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
  hide();
}

void SelectRectOverlay::show_rect(const QRect& rect, bool crossing) {
  crossing_ = crossing;
  setGeometry(rect.normalized());
  show();
  raise();
  update();
}

void SelectRectOverlay::hide_rect() { hide(); }

void SelectRectOverlay::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setBrush(Qt::NoBrush);
  if (crossing_) {
    p.setPen(QPen(QColor(46, 204, 113), 1, Qt::DashLine));
  } else {
    p.setPen(QPen(QColor(52, 152, 219), 1, Qt::SolidLine));
  }
  p.drawRect(rect().adjusted(0, 0, -1, -1));
}

}  // namespace brep::viewer
