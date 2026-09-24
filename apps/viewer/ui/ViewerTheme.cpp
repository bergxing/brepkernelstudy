#include "ui/ViewerTheme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

namespace brep::viewer
{
namespace
{

void Copy3(float dst[3], float r, float g, float b) noexcept
{
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
}

}  // namespace

void ViewerTheme::Resolve(float clearRgb[3], float wireRgb[3], float hoverRgb[3],
                          float previewRgb[3]) const noexcept
{
    if (Mode == ThemeMode::Light)
    {
        Copy3(clearRgb, 0.92f, 0.93f, 0.95f);
        Copy3(wireRgb, 0.18f, 0.20f, 0.24f);
        Copy3(previewRgb, 0.0f, 0.45f, 0.88f);
    }
    else
    {
        Copy3(clearRgb, 0.12f, 0.13f, 0.15f);
        // Bright wires on dark clear — black lines were invisible.
        Copy3(wireRgb, 0.78f, 0.80f, 0.84f);
        Copy3(previewRgb, 1.0f, 0.92f, 0.15f);
    }
    Copy3(hoverRgb, 0.25f, 0.85f, 1.0f);

    if (Invert)
    {
        const float c0 = clearRgb[0];
        const float c1 = clearRgb[1];
        const float c2 = clearRgb[2];
        clearRgb[0] = 1.0f - c0;
        clearRgb[1] = 1.0f - c1;
        clearRgb[2] = 1.0f - c2;
        wireRgb[0] = 1.0f - wireRgb[0];
        wireRgb[1] = 1.0f - wireRgb[1];
        wireRgb[2] = 1.0f - wireRgb[2];
        hoverRgb[0] = 1.0f - hoverRgb[0];
        hoverRgb[1] = 1.0f - hoverRgb[1];
        hoverRgb[2] = 1.0f - hoverRgb[2];
        previewRgb[0] = 1.0f - previewRgb[0];
        previewRgb[1] = 1.0f - previewRgb[1];
        previewRgb[2] = 1.0f - previewRgb[2];
    }
}

ViewerTheme LoadViewerTheme(QSettings& storage)
{
    const ViewerTheme defaults;
    storage.beginGroup(QStringLiteral("theme"));
    ViewerTheme theme;
    theme.Mode = static_cast<ThemeMode>(
        storage.value(QStringLiteral("mode"),
                      static_cast<int>(defaults.Mode))
            .toInt());
    theme.Invert =
        storage.value(QStringLiteral("invert"), defaults.Invert).toBool();
    storage.endGroup();
    if (theme.Mode != ThemeMode::Dark && theme.Mode != ThemeMode::Light)
    {
        theme.Mode = ThemeMode::Dark;
    }
    return theme;
}

void SaveViewerTheme(QSettings& storage, const ViewerTheme& theme)
{
    storage.beginGroup(QStringLiteral("theme"));
    storage.setValue(QStringLiteral("mode"), static_cast<int>(theme.Mode));
    storage.setValue(QStringLiteral("invert"), theme.Invert);
    storage.endGroup();
}

void ApplyQtTheme(QApplication& app, ThemeMode mode)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette pal;
    if (mode == ThemeMode::Light)
    {
        pal = QPalette();
        pal.setColor(QPalette::Window, QColor(245, 246, 248));
        pal.setColor(QPalette::WindowText, QColor(30, 32, 36));
        pal.setColor(QPalette::Base, QColor(255, 255, 255));
        pal.setColor(QPalette::AlternateBase, QColor(235, 237, 240));
        pal.setColor(QPalette::Text, QColor(30, 32, 36));
        pal.setColor(QPalette::Button, QColor(235, 237, 240));
        pal.setColor(QPalette::ButtonText, QColor(30, 32, 36));
        pal.setColor(QPalette::Highlight, QColor(50, 130, 220));
        pal.setColor(QPalette::HighlightedText, Qt::white);
    }
    else
    {
        pal.setColor(QPalette::Window, QColor(45, 47, 51));
        pal.setColor(QPalette::WindowText, QColor(220, 222, 225));
        pal.setColor(QPalette::Base, QColor(35, 37, 41));
        pal.setColor(QPalette::AlternateBase, QColor(50, 52, 56));
        pal.setColor(QPalette::Text, QColor(220, 222, 225));
        pal.setColor(QPalette::Button, QColor(55, 57, 62));
        pal.setColor(QPalette::ButtonText, QColor(220, 222, 225));
        pal.setColor(QPalette::Highlight, QColor(50, 130, 220));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::ToolTipBase, QColor(60, 62, 68));
        pal.setColor(QPalette::ToolTipText, QColor(220, 222, 225));
    }
    app.setPalette(pal);
}

}  // namespace brep::viewer
