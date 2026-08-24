#pragma once

#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QStringView>

namespace brep::viewer
{

/// Logical asset access backed by Qt resources (`:/assets/...`).
/// Call ensure_initialized() once at process start (SHARED DLL-safe).
class AssetCatalog
{
 public:
  static void ensure_initialized();

  /// Relative key under the /assets qrc prefix, e.g. "splash_xcad.png",
  /// "views/front.png".
  [[nodiscard]] static QString url(QStringView relative);

  [[nodiscard]] static bool exists(QStringView relative);
  [[nodiscard]] static QPixmap pixmap(QStringView relative);
  [[nodiscard]] static QIcon icon(QStringView relative);
};

}  // namespace brep::viewer
