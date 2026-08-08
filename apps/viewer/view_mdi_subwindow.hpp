#pragma once

#include <QMdiSubWindow>
#include <QRect>

namespace brep::viewer {

/// MDI subwindow that "maximizes" by filling the MDI viewport while keeping its
/// own title-bar min/max/close buttons (Qt's real maximize merges them away).
class ViewMdiSubWindow final : public QMdiSubWindow {
  Q_OBJECT
 public:
  explicit ViewMdiSubWindow(QWidget* parent = nullptr);

  /// Fill the MDI client area; keeps Normal window state so decorations remain.
  void fill_workspace();
  void clear_fill();
  [[nodiscard]] bool is_filled() const noexcept { return filled_; }
  void set_view_caption(char standard_view, int serial);
  [[nodiscard]] char standard_view() const noexcept { return standard_view_; }
  [[nodiscard]] int view_serial() const noexcept { return view_serial_; }

 protected:
  void changeEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void apply_fill_geometry();
  void remember_normal_geometry();
  void ensure_mdi_hooked();

  bool filled_{false};
  bool remember_geom_{true};
  bool was_filled_before_minimize_{false};
  char standard_view_{0};
  int view_serial_{0};
  QRect normal_geometry_{};
};

}  // namespace brep::viewer
