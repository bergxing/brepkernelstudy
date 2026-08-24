#include "commands/snap/SnapSettings.h"

#include <QSettings>

namespace brep::viewer::commands
{

std::uint32_t default_snap_kinds() noexcept
{
  return static_cast<std::uint32_t>(SnapKind::Endpoint) |
         static_cast<std::uint32_t>(SnapKind::Midpoint) |
         static_cast<std::uint32_t>(SnapKind::Center) |
         static_cast<std::uint32_t>(SnapKind::Intersection) |
         static_cast<std::uint32_t>(SnapKind::Perpendicular) |
         static_cast<std::uint32_t>(SnapKind::Nearest);
}

SnapSettings load_snap_settings(QSettings& storage)
{
  const SnapSettings defaults;
  storage.beginGroup(QStringLiteral("snap"));
  SnapSettings settings;
  settings.enabled =
      storage.value(QStringLiteral("enabled"), defaults.enabled).toBool();
  settings.kinds =
      storage.value(QStringLiteral("kinds"), defaults.kinds).toUInt();
  settings.aperture_px =
      storage.value(QStringLiteral("aperture_px"), defaults.aperture_px).toInt();
  settings.grid_enabled =
      storage.value(QStringLiteral("grid_enabled"), defaults.grid_enabled)
          .toBool();
  settings.grid_spacing =
      storage.value(QStringLiteral("grid_spacing"), defaults.grid_spacing)
          .toDouble();
  storage.endGroup();
  return settings;
}

void save_snap_settings(QSettings& storage, const SnapSettings& settings)
{
  storage.beginGroup(QStringLiteral("snap"));
  storage.setValue(QStringLiteral("enabled"), settings.enabled);
  storage.setValue(QStringLiteral("kinds"), settings.kinds);
  storage.setValue(QStringLiteral("aperture_px"), settings.aperture_px);
  storage.setValue(QStringLiteral("grid_enabled"), settings.grid_enabled);
  storage.setValue(QStringLiteral("grid_spacing"), settings.grid_spacing);
  storage.endGroup();
}

}  // namespace brep::viewer::commands
