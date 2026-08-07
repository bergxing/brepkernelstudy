#pragma once

#include <QMainWindow>

namespace brep::viewer {

/// Start page after splash (AutoCAD / MicroStation style): New opens workspace.
class HomeWindow final : public QMainWindow {
  Q_OBJECT
 public:
  explicit HomeWindow(QWidget* parent = nullptr);

 signals:
  void new_document_requested();
  void exit_requested();

 private:
  void build_ui();
};

}  // namespace brep::viewer
