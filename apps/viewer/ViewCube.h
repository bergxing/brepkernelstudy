#pragma once

#include "Camera.h"

#include <QColor>
#include <QWidget>

#include <functional>

namespace brep::viewer
{

/// CAD-style ViewCube overlay: shows orientation and snaps camera on click.
class ViewCubeWidget final : public QWidget
{
  Q_OBJECT
 public:
  explicit ViewCubeWidget(QWidget* parent = nullptr);

  void set_camera(Camera* camera) noexcept
  {
      m_camera = camera; 
  }
  void set_redraw_callback(std::function<void()> cb)
  {
      m_redraw = std::move(cb); 
  }

  enum class FaceId
  {
      None, Right, Left, Top, Bottom, Front, Back, Home 
  };

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  struct FaceGeom
  {
    FaceId id;
    char code;  // for Camera::set_standard_view
    const char* label;
    float nx, ny, nz;  // outward normal in world
    QColor color;
  };

  [[nodiscard]] FaceId hit_test(const QPoint& pos) const;
  void project_point(float x, float y, float z, float& sx, float& sy,
                     float& depth) const;
  void notify_redraw();

  Camera* m_camera{nullptr};
  std::function<void()> m_redraw;
  FaceId m_hover{FaceId::None};
  float m_lastYaw{0.0f};
  float m_lastPitch{0.0f};
  bool m_lastFramed{false};
  bool m_lastOrtho{false};
};

}  // namespace brep::viewer
