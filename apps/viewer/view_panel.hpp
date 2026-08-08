#pragma once

#include "camera.hpp"

#include <QWidget>

#include <functional>

class QCheckBox;

namespace brep::viewer {

/// Dock content: standard view orientation buttons (scheme A).
class ViewPanel final : public QWidget {
  Q_OBJECT
 public:
  explicit ViewPanel(QWidget* parent = nullptr);

  void set_camera(Camera* camera) noexcept { camera_ = camera; }
  void set_redraw_callback(std::function<void()> cb) {
    redraw_ = std::move(cb);
  }

  void sync_from_camera();

 private:
  void apply_view(char face);
  void on_ortho_toggled(bool checked);

  Camera* camera_{nullptr};
  std::function<void()> redraw_;
  QCheckBox* ortho_check_{nullptr};
};

}  // namespace brep::viewer
