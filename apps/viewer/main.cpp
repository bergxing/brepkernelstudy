#include "main_window.hpp"

#include "brep/log.hpp"

#include <QApplication>
#include <QMessageBox>

#include <exception>

int main(int argc, char* argv[]) {
  brep::init_logging("brep_viewer.log", brep::LogLevel::Info);

  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("brep_viewer"));

  try {
    brep::viewer::MainWindow window;
    window.show();
    return app.exec();
  } catch (const std::exception& ex) {
    BREP_ERROR("viewer failed: {}", ex.what());
    QMessageBox::critical(nullptr, QStringLiteral("Viewer Error"),
                          QString::fromUtf8(ex.what()));
    return 1;
  }
}
