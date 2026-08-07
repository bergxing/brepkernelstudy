#pragma once

#include <QPixmap>
#include <QWidget>

namespace brep::viewer {

/// Frameless XCAD splash; paints branded art and masks the corner watermark.
class SplashScreen final : public QWidget {
  Q_OBJECT
 public:
  explicit SplashScreen(QWidget* parent = nullptr);

  /// Load splash art from viewer assets (build dir or next to exe).
  bool load_artwork();

 signals:
  void finished();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void start_timer();
  void complete();

  QPixmap pixmap_;
  bool completed_{false};
};

}  // namespace brep::viewer
