#pragma once

class QApplication;
class QSettings;

namespace brep::viewer
{

enum class ThemeMode
{
    Dark = 0,
    Light = 1,
};

struct ViewerTheme
{
    ThemeMode Mode{ThemeMode::Dark};
    bool Invert{false};

    /// Resolved viewport colors (after Mode + Invert).
    void Resolve(float clearRgb[3], float wireRgb[3], float hoverRgb[3],
                 float previewRgb[3]) const noexcept;
};

[[nodiscard]] ViewerTheme LoadViewerTheme(QSettings& storage);
void SaveViewerTheme(QSettings& storage, const ViewerTheme& theme);
void ApplyQtTheme(QApplication& app, ThemeMode mode);

}  // namespace brep::viewer
