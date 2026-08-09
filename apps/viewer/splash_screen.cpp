#include "splash_screen.hpp"

#include "assets/asset_catalog.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>

namespace brep::viewer {
namespace {
constexpr int kSplashW = 800;
constexpr int kSplashH = 600;
}  // namespace

SplashScreen::SplashScreen(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::SplashScreen);
  setAttribute(Qt::WA_DeleteOnClose, false);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
  setFixedSize(kSplashW, kSplashH);
}

bool SplashScreen::load_artwork() {
  pixmap_ = AssetCatalog::pixmap(QStringLiteral("splash_xcad.png"));
  return !pixmap_.isNull();
}

void SplashScreen::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (const QScreen* screen = this->screen()) {
    const QRect geo = screen->availableGeometry();
    move(geo.center() - rect().center());
  }
  start_timer();
}

void SplashScreen::start_timer() {
  QTimer::singleShot(3000, this, [this] { complete(); });
}

void SplashScreen::complete() {
  if (completed_) return;
  completed_ = true;
  emit finished();
  close();
}

void SplashScreen::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Return ||
      event->key() == Qt::Key_Space) {
    complete();
    return;
  }
  QWidget::keyPressEvent(event);
}

void SplashScreen::mousePressEvent(QMouseEvent* event) {
  Q_UNUSED(event);
  complete();
}

void SplashScreen::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter p(this);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.fillRect(rect(), QColor(6, 18, 48));

  if (!pixmap_.isNull()) {
    // Fit entire image inside 800x600 — no cropping.
    const QPixmap scaled =
        pixmap_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint top_left((width() - scaled.width()) / 2,
                          (height() - scaled.height()) / 2);
    p.drawPixmap(top_left, scaled);
  } else {
    p.setPen(QColor(220, 230, 245));
    QFont f = font();
    f.setPointSize(36);
    f.setBold(true);
    p.setFont(f);
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("XCAD"));
  }
}

}  // namespace brep::viewer
