#include "home_window.hpp"
#include "main_window.hpp"
#include "splash_screen.hpp"

#include "brep/log.hpp"

#include <QApplication>
#include <QMessageBox>

#include <exception>

int main(int argc, char* argv[]) {
  brep::init_logging("brep_viewer.log", brep::LogLevel::Info);

  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("XCAD"));
  QApplication::setApplicationDisplayName(QStringLiteral("XCAD"));

  try {
    auto* splash = new brep::viewer::SplashScreen();
    if (!splash->load_artwork()) {
      BREP_WARN("splash artwork missing; showing fallback splash");
    }

    auto* home = new brep::viewer::HomeWindow();
    home->hide();

    QObject::connect(splash, &brep::viewer::SplashScreen::finished, &app,
                     [splash, home] {
                       home->show();
                       home->raise();
                       home->activateWindow();
                       splash->deleteLater();
                     });

    // Splash → start page → "新建" opens the modeling workspace.
    QObject::connect(
        home, &brep::viewer::HomeWindow::new_document_requested, &app, [home] {
          try {
            auto* workspace = new brep::viewer::MainWindow();
            workspace->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(workspace, &QObject::destroyed, home, [home] {
              home->show();
              home->raise();
              home->activateWindow();
            });
            home->hide();
            workspace->show();
            workspace->raise();
            workspace->activateWindow();
          } catch (const std::exception& ex) {
            BREP_ERROR("workspace failed: {}", ex.what());
            QMessageBox::critical(home, QStringLiteral("XCAD Error"),
                                  QString::fromUtf8(ex.what()));
            home->show();
          }
        });

    QObject::connect(home, &brep::viewer::HomeWindow::exit_requested, &app,
                     &QApplication::quit);

    splash->show();
    return app.exec();
  } catch (const std::exception& ex) {
    BREP_ERROR("viewer failed: {}", ex.what());
    QMessageBox::critical(nullptr, QStringLiteral("XCAD Error"),
                          QString::fromUtf8(ex.what()));
    return 1;
  }
}
