#pragma once

#include "commands/snap/SnapSettings.h"

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

namespace brep::viewer
{

class SnapSettingsDialog final : public QDialog
{
  Q_OBJECT

 public:
  explicit SnapSettingsDialog(const commands::SnapSettings& settings,
                              QWidget* parent = nullptr);

  [[nodiscard]] commands::SnapSettings snap_settings() const;

 private:
  QCheckBox* m_enabled{nullptr};
  QCheckBox* m_endpoint{nullptr};
  QCheckBox* m_midpoint{nullptr};
  QCheckBox* m_center{nullptr};
  QCheckBox* m_intersection{nullptr};
  QCheckBox* m_perpendicular{nullptr};
  QCheckBox* m_nearest{nullptr};
  QCheckBox* m_grid{nullptr};
  QSpinBox* m_aperture{nullptr};
  QDoubleSpinBox* m_gridSpacing{nullptr};
};

}  // namespace brep::viewer
