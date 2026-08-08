#pragma once

#include <QRect>
#include <QWidget>

namespace brep::viewer {

/// Screen-space box-select rectangle drawn above a Vulkan window container.
/// Covers the full host and paints only a wire outline (no fill).
class SelectRectOverlay final : public QWidget {
 public:
  explicit SelectRectOverlay(QWidget* parent = nullptr);

  void show_rect(const QRect& rect, bool crossing);
  void hide_rect();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QRect select_rect_{};
  bool crossing_{false};
};

}  // namespace brep::viewer
