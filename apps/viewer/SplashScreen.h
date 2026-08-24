#pragma once

#include <QPixmap>
#include <QWidget>

namespace brep::viewer
{

/// Frameless 800x600 splash; shows full artwork for ~3s then emits finished().
class SplashScreen final : public QWidget
{
  Q_OBJECT
 public:
  explicit SplashScreen(QWidget* parent = nullptr);

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

  QPixmap m_pixmap;
  bool m_completed{false};
};

}  // namespace brep::viewer
