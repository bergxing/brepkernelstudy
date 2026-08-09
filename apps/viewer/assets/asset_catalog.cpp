#include "assets/asset_catalog.hpp"

#include <QFile>

// File-scope (no C++ namespace): Q_INIT_RESOURCE must resolve to the
// global qInitResources_assets() emitted by rcc into viewer_ui.
static void init_viewer_assets_resource() {
  Q_INIT_RESOURCE(assets);
}

static bool g_assets_initialized = false;

namespace brep::viewer {

void AssetCatalog::ensure_initialized() {
  if (g_assets_initialized) return;
  init_viewer_assets_resource();
  g_assets_initialized = true;
}

QString AssetCatalog::url(QStringView relative) {
  ensure_initialized();
  return QStringLiteral(":/assets/%1").arg(relative);
}

bool AssetCatalog::exists(QStringView relative) {
  return QFile::exists(url(relative));
}

QPixmap AssetCatalog::pixmap(QStringView relative) {
  const QString path = url(relative);
  if (!QFile::exists(path)) return {};
  QPixmap pm(path);
  return pm.isNull() ? QPixmap{} : pm;
}

QIcon AssetCatalog::icon(QStringView relative) {
  const QPixmap pm = pixmap(relative);
  return pm.isNull() ? QIcon{} : QIcon(pm);
}

}  // namespace brep::viewer
