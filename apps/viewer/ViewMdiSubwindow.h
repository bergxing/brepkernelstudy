#pragma once

#include <QMdiSubWindow>
#include <QRect>

namespace brep::viewer
{

/// MDI subwindow that "maximizes" by filling the MDI viewport while keeping its
/// own title-bar min/max/close buttons (Qt's real maximize merges them away).
class ViewMdiSubWindow final : public QMdiSubWindow
{
  Q_OBJECT
 public:
  explicit ViewMdiSubWindow(QWidget* parent = nullptr);

  /// Fill the MDI client area; keeps Normal window state so decorations remain.
  void fill_workspace();
  void clear_fill();
  [[nodiscard]] bool is_filled() const noexcept
  {
      return m_filled; 
  }
  void set_view_caption(char standard_view, int serial);
  [[nodiscard]] char standard_view() const noexcept
  {
      return m_standardView; 
  }
  [[nodiscard]] int view_serial() const noexcept
  {
      return m_viewSerial; 
  }

 protected:
  void changeEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void apply_fill_geometry();
  void remember_normal_geometry();
  void ensure_mdi_hooked();

  bool m_filled{false};
  bool m_rememberGeom{true};
  bool m_wasFilledBeforeMinimize{false};
  char m_standardView{0};
  int m_viewSerial{0};
  QRect m_normalGeometry{};
};

}  // namespace brep::viewer
