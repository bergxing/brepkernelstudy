#include "ui/snap_settings_dialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace brep::viewer {
namespace {

bool has_kind(const commands::SnapSettings& settings, SnapKind kind) {
  return (settings.kinds & static_cast<std::uint32_t>(kind)) != 0;
}

void set_kind(std::uint32_t& kinds, SnapKind kind, bool enabled) {
  const auto bit = static_cast<std::uint32_t>(kind);
  if (enabled) {
    kinds |= bit;
  } else {
    kinds &= ~bit;
  }
}

}  // namespace

SnapSettingsDialog::SnapSettingsDialog(
    const commands::SnapSettings& settings, QWidget* parent)
    : QDialog(parent) {
  setWindowTitle(tr("Snap Settings"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  enabled_ = new QCheckBox(tr("Enable AccuSnap"), this);
  enabled_->setChecked(settings.enabled);
  layout->addWidget(enabled_);

  auto* modes = new QGroupBox(tr("Snap modes"), this);
  auto* modes_layout = new QVBoxLayout(modes);
  endpoint_ = new QCheckBox(tr("Endpoint (E)"), modes);
  midpoint_ = new QCheckBox(tr("Midpoint (M)"), modes);
  center_ = new QCheckBox(tr("Center (C)"), modes);
  intersection_ = new QCheckBox(tr("Intersection (I)"), modes);
  perpendicular_ = new QCheckBox(tr("Perpendicular (P)"), modes);
  nearest_ = new QCheckBox(tr("Nearest"), modes);
  grid_ = new QCheckBox(tr("Grid (G)"), modes);
  endpoint_->setChecked(has_kind(settings, SnapKind::Endpoint));
  midpoint_->setChecked(has_kind(settings, SnapKind::Midpoint));
  center_->setChecked(has_kind(settings, SnapKind::Center));
  intersection_->setChecked(has_kind(settings, SnapKind::Intersection));
  perpendicular_->setChecked(has_kind(settings, SnapKind::Perpendicular));
  nearest_->setChecked(has_kind(settings, SnapKind::Nearest));
  grid_->setChecked(settings.grid_enabled);
  for (QCheckBox* checkbox :
       {endpoint_, midpoint_, center_, intersection_, perpendicular_, nearest_,
        grid_}) {
    modes_layout->addWidget(checkbox);
  }
  layout->addWidget(modes);

  auto* values = new QFormLayout;
  aperture_ = new QSpinBox(this);
  aperture_->setRange(4, 40);
  aperture_->setSuffix(tr(" px"));
  aperture_->setValue(settings.aperture_px);
  values->addRow(tr("Aperture:"), aperture_);

  grid_spacing_ = new QDoubleSpinBox(this);
  grid_spacing_->setRange(0.001, 1000000.0);
  grid_spacing_->setDecimals(3);
  grid_spacing_->setValue(settings.grid_spacing);
  grid_spacing_->setEnabled(settings.grid_enabled);
  values->addRow(tr("Grid spacing:"), grid_spacing_);
  connect(grid_, &QCheckBox::toggled, grid_spacing_,
          &QDoubleSpinBox::setEnabled);
  layout->addLayout(values);

  auto* accudraw =
      new QCheckBox(tr("Dynamic input (AccuDraw, coming soon)"), this);
  accudraw->setEnabled(false);
  layout->addWidget(accudraw);

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
}

commands::SnapSettings SnapSettingsDialog::snap_settings() const {
  commands::SnapSettings settings;
  settings.enabled = enabled_->isChecked();
  settings.kinds = 0;
  set_kind(settings.kinds, SnapKind::Endpoint, endpoint_->isChecked());
  set_kind(settings.kinds, SnapKind::Midpoint, midpoint_->isChecked());
  set_kind(settings.kinds, SnapKind::Center, center_->isChecked());
  set_kind(settings.kinds, SnapKind::Intersection,
           intersection_->isChecked());
  set_kind(settings.kinds, SnapKind::Perpendicular,
           perpendicular_->isChecked());
  set_kind(settings.kinds, SnapKind::Nearest, nearest_->isChecked());
  settings.aperture_px = aperture_->value();
  settings.grid_enabled = grid_->isChecked();
  settings.grid_spacing = grid_spacing_->value();
  return settings;
}

}  // namespace brep::viewer
