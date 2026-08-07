#include "splash_screen.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>

namespace brep::viewer {

SplashScreen::SplashScreen(QWidget* parent) : QWidget(parent) {
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SplashScreen);
  setAttribute(Qt::WA_DeleteOnClose, false);
  setFixedSize(960, 540);
}

bool SplashScreen::load_artwork() {
  QStringList candidates;
  candidates << QStringLiteral(BREP_VIEWER_ASSETS_DIR "/splash_xcad.png");
  candidates << QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("assets/splash_xcad.png"));
  candidates << QDir(QCoreApplication::applicationDirPath())
                    .filePath(QStringLiteral("../assets/splash_xcad.png"));

  for (const QString& path : candidates) {
    if (QFileInfo::exists(path)) {
      pixmap_ = QPixmap(path);
      if (!pixmap_.isNull()) return true;
    }
  }
  return false;
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

  if (!pixmap_.isNull()) {
    const QPixmap scaled =
        pixmap_.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QPoint top_left((width() - scaled.width()) / 2,
                          (height() - scaled.height()) / 2);
    p.drawPixmap(top_left, scaled);

    // Cover Doubao watermark in the source art (bottom-right corner).
    const int cover_w = qMax(160, width() / 4);
    const int cover_h = qMax(40, height() / 16);
    p.fillRect(QRect(width() - cover_w, height() - cover_h, cover_w, cover_h),
               QColor(8, 22, 58));
  } else {
    p.fillRect(rect(), QColor(8, 24, 64));
    p.setPen(Qt::white);
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("XCAD"));
  }
}

}  // namespace brep::viewer
