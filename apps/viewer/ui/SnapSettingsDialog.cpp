#include "ui/SnapSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace brep::viewer
{
namespace
{

bool has_kind(const commands::SnapSettings& settings, SnapKind kind)
{
  return (settings.kinds & static_cast<std::uint32_t>(kind)) != 0;
}

void set_kind(std::uint32_t& kinds, SnapKind kind, bool enabled)
{
  const auto bit = static_cast<std::uint32_t>(kind);
  if (enabled)
  {
    kinds |= bit;
  }
  else
  {
    kinds &= ~bit;
  }
}

}  // namespace

SnapSettingsDialog::SnapSettingsDialog(
    const commands::SnapSettings& settings, QWidget* parent)
    : QDialog(parent)
    {
  setWindowTitle(tr("Snap Settings"));
  setModal(true);

  auto* layout = new QVBoxLayout(this);
  m_enabled = new QCheckBox(tr("Enable AccuSnap"), this);
  m_enabled->setChecked(settings.enabled);
  layout->addWidget(m_enabled);

  auto* modes = new QGroupBox(tr("Snap modes"), this);
  auto* modes_layout = new QVBoxLayout(modes);
  m_endpoint = new QCheckBox(tr("Endpoint (E)"), modes);
  m_midpoint = new QCheckBox(tr("Midpoint (M)"), modes);
  m_center = new QCheckBox(tr("Center (C)"), modes);
  m_intersection = new QCheckBox(tr("Intersection (I)"), modes);
  m_perpendicular = new QCheckBox(tr("Perpendicular (P)"), modes);
  m_nearest = new QCheckBox(tr("Nearest"), modes);
  m_grid = new QCheckBox(tr("Grid (G)"), modes);
  m_endpoint->setChecked(has_kind(settings, SnapKind::Endpoint));
  m_midpoint->setChecked(has_kind(settings, SnapKind::Midpoint));
  m_center->setChecked(has_kind(settings, SnapKind::Center));
  m_intersection->setChecked(has_kind(settings, SnapKind::Intersection));
  m_perpendicular->setChecked(has_kind(settings, SnapKind::Perpendicular));
  m_nearest->setChecked(has_kind(settings, SnapKind::Nearest));
  m_grid->setChecked(settings.grid_enabled);
  for (QCheckBox* checkbox :
       {m_endpoint, m_midpoint, m_center, m_intersection, m_perpendicular, m_nearest,
        m_grid})
       {
    modes_layout->addWidget(checkbox);
  }
  layout->addWidget(modes);

  auto* values = new QFormLayout;
  m_aperture = new QSpinBox(this);
  m_aperture->setRange(4, 40);
  m_aperture->setSuffix(tr(" px"));
  m_aperture->setValue(settings.aperture_px);
  values->addRow(tr("Aperture:"), m_aperture);

  m_gridSpacing = new QDoubleSpinBox(this);
  m_gridSpacing->setRange(0.001, 1000000.0);
  m_gridSpacing->setDecimals(3);
  m_gridSpacing->setValue(settings.grid_spacing);
  m_gridSpacing->setEnabled(settings.grid_enabled);
  values->addRow(tr("Grid spacing:"), m_gridSpacing);
  connect(m_grid, &QCheckBox::toggled, m_gridSpacing,
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

commands::SnapSettings SnapSettingsDialog::snap_settings() const
{
  commands::SnapSettings settings;
  settings.enabled = m_enabled->isChecked();
  settings.kinds = 0;
  set_kind(settings.kinds, SnapKind::Endpoint, m_endpoint->isChecked());
  set_kind(settings.kinds, SnapKind::Midpoint, m_midpoint->isChecked());
  set_kind(settings.kinds, SnapKind::Center, m_center->isChecked());
  set_kind(settings.kinds, SnapKind::Intersection,
           m_intersection->isChecked());
  set_kind(settings.kinds, SnapKind::Perpendicular,
           m_perpendicular->isChecked());
  set_kind(settings.kinds, SnapKind::Nearest, m_nearest->isChecked());
  settings.aperture_px = m_aperture->value();
  settings.grid_enabled = m_grid->isChecked();
  settings.grid_spacing = m_gridSpacing->value();
  return settings;
}

}  // namespace brep::viewer
