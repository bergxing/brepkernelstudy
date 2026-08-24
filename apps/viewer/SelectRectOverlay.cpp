#include "SelectRectOverlay.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QRegion>

#include <algorithm>

namespace brep::viewer
{
namespace
{

/// Visible widget region = only the four border strips (no interior).
QRegion border_mask(const QRect& r, int thickness = 2)
{
  if (r.width() < 1 || r.height() < 1) return {};
  const int t = std::max(1, thickness);
  QRegion region;
  region += QRect(r.left(), r.top(), r.width(), t);
  region += QRect(r.left(), r.bottom() - t + 1, r.width(), t);
  region += QRect(r.left(), r.top(), t, r.height());
  region += QRect(r.right() - t + 1, r.top(), t, r.height());
  return region;
}

}  // namespace

SelectRectOverlay::SelectRectOverlay(QWidget* parent) : QWidget(parent)
{
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setAutoFillBackground(false);
  setStyleSheet(QStringLiteral("background: transparent;"));
  QPalette pal = palette();
  pal.setBrush(QPalette::Window, Qt::NoBrush);
  pal.setColor(QPalette::Window, Qt::transparent);
  setPalette(pal);
  hide();
}

void SelectRectOverlay::show_rect(const QRect& rect, bool crossing)
{
  m_crossing = crossing;
  m_selectRect = rect.normalized();
  if (QWidget* host = parentWidget())
  {
    setGeometry(host->rect());
  }
  // Mask out the interior so Windows cannot paint an opaque fill there.
  setMask(border_mask(m_selectRect));
  show();
  raise();
  update();
}

void SelectRectOverlay::hide_rect()
{
  clearMask();
  hide();
}

void SelectRectOverlay::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  if (m_selectRect.width() < 1 || m_selectRect.height() < 1) return;

  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setBrush(Qt::NoBrush);
  if (m_crossing)
  {
    p.setPen(QPen(QColor(46, 204, 113), 1, Qt::DashLine));
  }
  else
  {
    p.setPen(QPen(QColor(52, 152, 219), 1, Qt::SolidLine));
  }
  p.drawRect(m_selectRect.adjusted(0, 0, -1, -1));
}

}  // namespace brep::viewer
