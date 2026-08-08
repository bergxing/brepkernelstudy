#include "home_window.hpp"
#include "i18n/language_manager.hpp"
#include "main_window.hpp"
#include "splash_screen.hpp"

#include "brep/log.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QString>

#include <exception>

namespace {

void open_home_window(QApplication& app);

void open_workspace(QApplication& app, const QString& path = {}) {
  try {
    auto* workspace = new brep::viewer::MainWindow();
    workspace->setAttribute(Qt::WA_DeleteOnClose);
    // Closing the workspace ends the application (do not return to Home).

    workspace->show();
    workspace->raise();
    workspace->activateWindow();
    app.setQuitOnLastWindowClosed(true);

    if (!path.isEmpty()) {
      if (!workspace->open_document(path)) {
        BREP_WARN("failed to open document from home: {}",
                  path.toStdString());
      }
    }
  } catch (const std::exception& ex) {
    BREP_ERROR("workspace failed: {}", ex.what());
    QMessageBox::critical(nullptr, QStringLiteral("XCAD Error"),
                          QString::fromUtf8(ex.what()));
    app.setQuitOnLastWindowClosed(true);
    open_home_window(app);
  }
}

void open_home_window(QApplication& app) {
  auto* home = new brep::viewer::HomeWindow();
  home->setAttribute(Qt::WA_DeleteOnClose);

  QObject::connect(home, &brep::viewer::HomeWindow::exit_requested, &app,
                   &QApplication::quit);

  QObject::connect(
      home, &brep::viewer::HomeWindow::new_document_requested, &app,
      [home, &app] {
        // Close home first; open workspace only after home is gone.
        app.setQuitOnLastWindowClosed(false);
        QObject::connect(home, &QObject::destroyed, &app,
                         [&app] { open_workspace(app); },
                         Qt::QueuedConnection);
        home->close();
      });

  QObject::connect(
      home, &brep::viewer::HomeWindow::open_document_requested, &app,
      [home, &app](const QString& path) {
        app.setQuitOnLastWindowClosed(false);
        QObject::connect(
            home, &QObject::destroyed, &app,
            [path, &app] { open_workspace(app, path); },
            Qt::QueuedConnection);
        home->close();
      });

  home->show();
  home->raise();
  home->activateWindow();
  app.setQuitOnLastWindowClosed(true);
}

}  // namespace

int main(int argc, char* argv[]) {
  brep::init_logging("brep_viewer.log", brep::LogLevel::Info);

  QApplication app(argc, argv);
  QApplication::setOrganizationName(QStringLiteral("XCAD"));
  QApplication::setApplicationName(QStringLiteral("XCAD"));
  QApplication::setApplicationDisplayName(QStringLiteral("XCAD"));

  auto& languages = brep::viewer::LanguageManager::instance();
  languages.load_preference_from_settings();
  languages.apply(languages.preference());

  try {
    auto* splash = new brep::viewer::SplashScreen();
    if (!splash->load_artwork()) {
      BREP_WARN("splash artwork missing; showing fallback splash");
    }

    QObject::connect(splash, &brep::viewer::SplashScreen::finished, &app,
                     [splash, &app] {
                       open_home_window(app);
                       splash->deleteLater();
                     });

    splash->show();
    return app.exec();
  } catch (const std::exception& ex) {
    BREP_ERROR("viewer failed: {}", ex.what());
    QMessageBox::critical(nullptr, QStringLiteral("XCAD Error"),
                          QString::fromUtf8(ex.what()));
    return 1;
  }
}
