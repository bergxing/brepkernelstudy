#include "main_window.hpp"
#include "splash_screen.hpp"

#include "brep/log.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QTimer>

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
    splash->show();

    // Build the main window while the splash is visible, then reveal after 3s.
    auto* window = new brep::viewer::MainWindow();
    window->hide();

    QObject::connect(splash, &brep::viewer::SplashScreen::finished, &app,
                     [splash, window] {
                       window->show();
                       window->raise();
                       window->activateWindow();
                       splash->deleteLater();
                     });

    return app.exec();
  } catch (const std::exception& ex) {
    BREP_ERROR("viewer failed: {}", ex.what());
    QMessageBox::critical(nullptr, QStringLiteral("XCAD Error"),
                          QString::fromUtf8(ex.what()));
    return 1;
  }
}
