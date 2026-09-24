#include "app/ViewerApp.h"

#include "app/ApplicationContext.h"
#include "assets/AssetCatalog.h"
#include "bootstrap/AppConfigLoader.h"
#include "bootstrap/ApplicationContainer.h"
#include "HomeWindow.h"
#include "i18n/LanguageManager.h"
#include "MainWindow.h"
#include "SplashScreen.h"

#include "api/Core.h"

#include "SARibbonBar.h"

#include <QApplication>
#include <QMessageBox>
#include <QString>

#include <exception>
#include <memory>

namespace brep::viewer
{
namespace
{

void open_home_window(QApplication& app, ApplicationContext& appContext);

void open_workspace(QApplication& app, ApplicationContext& appContext,
                    const QString& path = {})
{
  try {
    auto* workspace = new MainWindow(appContext);
    workspace->setAttribute(Qt::WA_DeleteOnClose);

    workspace->show();
    workspace->raise();
    workspace->activateWindow();
    app.setQuitOnLastWindowClosed(true);

    if (!path.isEmpty())
    {
      if (!workspace->open_document(path))
      {
        BREP_WARN("failed to open document from home: {}",
                  path.toStdString());
      }
    }
  } catch (const std::exception& ex)
  {
    BREP_ERROR("workspace failed: {}", ex.what());
    QMessageBox::critical(nullptr, QStringLiteral("XCAD Error"),
                          QString::fromUtf8(ex.what()));
    app.setQuitOnLastWindowClosed(true);
    open_home_window(app, appContext);
  }
}

void open_home_window(QApplication& app, ApplicationContext& appContext)
{
  auto* home = new HomeWindow();
  home->setAttribute(Qt::WA_DeleteOnClose);

  QObject::connect(home, &HomeWindow::exit_requested, &app, &QApplication::quit);

  QObject::connect(
      home, &HomeWindow::new_document_requested, &app,
      [home, &app, &appContext] {
        app.setQuitOnLastWindowClosed(false);
        QObject::connect(home, &QObject::destroyed, &app,
                         [&app, &appContext] { open_workspace(app, appContext); },
                         Qt::QueuedConnection);
        home->close();
      });

  QObject::connect(
      home, &HomeWindow::open_document_requested, &app,
      [home, &app, &appContext](const QString& path) {
        app.setQuitOnLastWindowClosed(false);
        QObject::connect(
            home, &QObject::destroyed, &app,
            [path, &app, &appContext] { open_workspace(app, appContext, path); },
            Qt::QueuedConnection);
        home->close();
      });

  home->show();
  home->raise();
  home->activateWindow();
  app.setQuitOnLastWindowClosed(true);
}

}  // namespace

int run_viewer(int argc, char* argv[])
{
  ApplicationContext appContext;
  appContext.config = bootstrap::LoadAppConfig(argc, argv);

  ::brep::InitLogging(appContext.config.logPath.c_str(), ::brep::LogLevel::Info);
  AssetCatalog::ensure_initialized();

  SARibbonBar::initHighDpi();
  QApplication app(argc, argv);
  QApplication::setOrganizationName(QStringLiteral("XCAD"));
  QApplication::setApplicationName(QStringLiteral("XCAD"));
  QApplication::setApplicationDisplayName(QStringLiteral("XCAD"));

  auto& languages = LanguageManager::instance();
  languages.load_preference_from_settings();
  languages.apply(languages.preference());

  appContext.container = bootstrap::BuildApplicationContainer(appContext.config);

  try {
    auto* splash = new SplashScreen();
    if (!splash->load_artwork())
    {
      BREP_WARN("splash artwork missing; showing fallback splash");
    }

    QObject::connect(splash, &SplashScreen::finished, &app,
                     [splash, &app, &appContext] {
                       open_home_window(app, appContext);
                       splash->deleteLater();
                     });

    splash->show();
    return app.exec();
  } catch (const std::exception& ex)
  {
    BREP_ERROR("viewer failed: {}", ex.what());
    QMessageBox::critical(nullptr, QStringLiteral("XCAD Error"),
                          QString::fromUtf8(ex.what()));
    return 1;
  }
}

}  // namespace brep::viewer
