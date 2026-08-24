#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>

namespace brep::viewer
{

/// Loads .qm translators and notifies UI to retranslate (hot language switch).
class LanguageManager final : public QObject
{
  Q_OBJECT
 public:
  static LanguageManager& instance();

  /// Preference: "system" | "zh_CN" | "en"
  [[nodiscard]] QString preference() const
  {
      return m_preference; 
  }
  [[nodiscard]] QString LoadedTag() const
  {
      return m_loadedTag; 
  }

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

  QTranslator m_appTranslator;
  QTranslator m_qtTranslator;
  QString m_preference{QStringLiteral("system")};
  QString m_loadedTag;
};

}  // namespace brep::viewer
