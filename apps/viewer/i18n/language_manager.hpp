#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>

namespace brep::viewer {

/// Loads .qm translators and notifies UI to retranslate (hot language switch).
class LanguageManager final : public QObject {
  Q_OBJECT
 public:
  static LanguageManager& instance();

  /// Preference: "system" | "zh_CN" | "en"
  [[nodiscard]] QString preference() const { return preference_; }
  [[nodiscard]] QString loaded_tag() const { return loaded_tag_; }

  /// Resolve preference to an actual language tag (zh_CN / en).
  [[nodiscard]] static QString resolve_tag(const QString& preference);

  void load_preference_from_settings();
  void set_preference(const QString& preference);

  /// Install translators for tag and emit languageChanged.
  bool apply(const QString& preference_or_tag);

 signals:
  void languageChanged(const QString& loaded_tag);

 private:
  explicit LanguageManager(QObject* parent = nullptr);

  [[nodiscard]] QString translations_dir() const;
  bool install_for_tag(const QString& tag);

  QTranslator app_translator_;
  QTranslator qt_translator_;
  QString preference_{QStringLiteral("system")};
  QString loaded_tag_;
};

}  // namespace brep::viewer
