#pragma once

#include "commands/snap/snap_settings.hpp"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

namespace brep::viewer {

class SnapSettingsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit SnapSettingsDialog(const commands::SnapSettings& settings,
                              QWidget* parent = nullptr);

  [[nodiscard]] commands::SnapSettings snap_settings() const;

 private:
  QCheckBox* enabled_{nullptr};
  QCheckBox* endpoint_{nullptr};
  QCheckBox* midpoint_{nullptr};
  QCheckBox* center_{nullptr};
  QCheckBox* intersection_{nullptr};
  QCheckBox* perpendicular_{nullptr};
  QCheckBox* nearest_{nullptr};
  QCheckBox* grid_{nullptr};
  QSpinBox* aperture_{nullptr};
  QDoubleSpinBox* grid_spacing_{nullptr};
};

}  // namespace brep::viewer
