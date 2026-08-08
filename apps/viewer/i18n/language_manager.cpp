#include "i18n/language_manager.hpp"

#include "brep/log.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>

namespace brep::viewer {

LanguageManager& LanguageManager::instance() {
  static LanguageManager mgr;
  return mgr;
}

LanguageManager::LanguageManager(QObject* parent) : QObject(parent) {}

QString LanguageManager::resolve_tag(const QString& preference) {
  if (preference == QLatin1String("zh_CN") ||
      preference == QLatin1String("en")) {
    return preference;
  }
  // system or unknown → map from OS UI language
  const QString sys = QLocale::system().name();  // e.g. zh_CN, en_US
  if (sys.startsWith(QLatin1String("zh"), Qt::CaseInsensitive)) {
    return QStringLiteral("zh_CN");
  }
  if (sys.startsWith(QLatin1String("en"), Qt::CaseInsensitive)) {
    return QStringLiteral("en");
  }
  return QStringLiteral("zh_CN");
}

QString LanguageManager::translations_dir() const {
  const QString app_dir = QCoreApplication::applicationDirPath();
  const QString beside = app_dir + QStringLiteral("/translations");
  if (QDir(beside).exists()) return beside;
  // Dev fallback: source tree next to build
  const QString src = QStringLiteral(BREP_VIEWER_I18N_DIR);
  if (QDir(src).exists()) return src;
  return beside;
}

void LanguageManager::load_preference_from_settings() {
  QSettings settings;
  preference_ =
      settings.value(QStringLiteral("ui/language"), QStringLiteral("system"))
          .toString();
  if (preference_.isEmpty()) preference_ = QStringLiteral("system");
}

void LanguageManager::set_preference(const QString& preference) {
  preference_ = preference.isEmpty() ? QStringLiteral("system") : preference;
  QSettings settings;
  settings.setValue(QStringLiteral("ui/language"), preference_);
  apply(preference_);
}

bool LanguageManager::apply(const QString& preference_or_tag) {
  if (preference_or_tag == QLatin1String("system") ||
      preference_or_tag == QLatin1String("zh_CN") ||
      preference_or_tag == QLatin1String("en")) {
    preference_ = preference_or_tag;
  }
  const QString tag = resolve_tag(preference_);
  const bool ok = install_for_tag(tag);
  loaded_tag_ = tag;
  BREP_INFO("LanguageManager applied preference={} tag={} ok={}",
            preference_.toStdString(), tag.toStdString(), ok);
  emit languageChanged(loaded_tag_);
  return ok;
}

bool LanguageManager::install_for_tag(const QString& tag) {
  auto* app = QCoreApplication::instance();
  if (!app) return false;

  app->removeTranslator(&app_translator_);
  app->removeTranslator(&qt_translator_);

  // English = source language: no app .qm required.
  bool app_ok = true;
  if (tag != QLatin1String("en")) {
    const QString qm =
        translations_dir() + QStringLiteral("/xcad_%1.qm").arg(tag);
    app_ok = app_translator_.load(qm);
    if (app_ok) {
      app->installTranslator(&app_translator_);
      BREP_INFO("loaded app translator '{}'", qm.toStdString());
    } else {
      BREP_WARN("failed to load app translator '{}'", qm.toStdString());
    }
  } else {
    BREP_INFO("using source language English (no app .qm)");
  }

  // Optional: Qt built-in dialog buttons etc.
  const QString qt_qm_dir =
      QLibraryInfo::path(QLibraryInfo::TranslationsPath);
  const QString qt_qm =
      qt_qm_dir + QStringLiteral("/qtbase_%1.qm").arg(tag);
  if (qt_translator_.load(qt_qm) ||
      qt_translator_.load(QStringLiteral("qtbase_%1").arg(tag), qt_qm_dir)) {
    app->installTranslator(&qt_translator_);
  }

  return app_ok;
}

}  // namespace brep::viewer
