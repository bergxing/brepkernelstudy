#include "splash_screen.hpp"

#include <QApplication>
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
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::SplashScreen);
  setAttribute(Qt::WA_DeleteOnClose, false);
  setAttribute(Qt::WA_OpaquePaintEvent, true);
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
      if (!pixmap_.isNull()) {
        fit_to_artwork();
        return true;
      }
    }
  }
  setFixedSize(960, 540);
  return false;
}

void SplashScreen::fit_to_artwork() {
  if (pixmap_.isNull()) {
    setFixedSize(960, 540);
    return;
  }

  // Show the entire image (letterbox if needed). Cap to ~80% of screen.
  QSize target = pixmap_.size();
  if (const QScreen* screen = QApplication::primaryScreen()) {
    const QSize avail = screen->availableGeometry().size() * 4 / 5;
    target = target.scaled(avail, Qt::KeepAspectRatio);
  }
  // Prefer exact aspect of source so "CAD" / torus are not cropped.
  setFixedSize(target);
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
    // KeepAspectRatio: never crop — full splash art visible.
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
