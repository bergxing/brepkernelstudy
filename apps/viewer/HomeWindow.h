#pragma once

#include <QMainWindow>
#include <QString>

namespace brep::viewer
{

/// Start page after splash (AutoCAD / MicroStation style): New / Open workspace.
class HomeWindow final : public QMainWindow
{
  Q_OBJECT
 public:
  explicit HomeWindow(QWidget* parent = nullptr);

 signals:
  void new_document_requested();
  void open_document_requested(const QString& path);
  void exit_requested();

 private slots:
  void on_open_clicked();

 private:
  void build_ui();
};

}  // namespace brep::viewer
