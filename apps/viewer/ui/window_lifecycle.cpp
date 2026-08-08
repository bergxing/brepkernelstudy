#include "main_window.hpp"

#include "brep/log.hpp"
#include "ecs/components.hpp"
#include "ecs/systems.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStatusBar>

namespace brep::viewer {

bool MainWindow::open_document(const QString& path) {
  auto ctx = make_command_context();
  const auto result = commands::open_xl_file(ctx, path);
  if (!result.succeeded()) {
    if (!result.message.isEmpty()) {
      statusBar()->showMessage(result.message, 6000);
    }
    return false;
  }

  if (auto* vw = active_vulkan_window()) {
    const float aspect =
        float(std::max(1, vw->width())) / float(std::max(1, vw->height()));
    ecs::fit_camera_to_scene(world_.registry(), vw->camera(), aspect);
  }
  update_property_panel(entt::null);
  request_all_views_update();
  refresh_window_title();
  statusBar()->showMessage(result.message, 6000);
  return true;
}

void MainWindow::refresh_window_title() {
  setWindowTitle(document_.window_title());
}

bool MainWindow::confirm_close_or_save() {
  BREP_INFO("confirm_close_or_save dirty={} tool={}", document_.dirty(),
            command_manager_.has_active_tool());
  if (command_manager_.has_active_tool()) {
    auto ctx = make_command_context();
    command_manager_.cancel_active_tool(ctx);
    sync_tool_ui();
  }

  if (!document_.dirty()) {
    BREP_INFO("confirm_close_or_save: clean document, allow close");
    return true;
  }

  QMessageBox box(this);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(tr("Save Document"));
  box.setText(tr("Document \"%1\" has been modified. Save changes?")
                  .arg(document_.title()));
  box.setInformativeText(tr(
      "Choose Save to write the file and exit; Discard to exit without "
      "saving; Cancel to keep editing."));
  QAbstractButton* btn_save =
      box.addButton(tr("Save"), QMessageBox::AcceptRole);
  QAbstractButton* btn_discard =
      box.addButton(tr("Don't Save"), QMessageBox::DestructiveRole);
  QAbstractButton* btn_cancel =
      box.addButton(tr("Cancel"), QMessageBox::RejectRole);
  box.setDefaultButton(qobject_cast<QPushButton*>(btn_save));
  box.setEscapeButton(btn_cancel);
  box.exec();

  if (box.clickedButton() == btn_cancel) {
    BREP_INFO("confirm_close_or_save: user cancelled");
    return false;
  }
  if (box.clickedButton() == btn_discard) {
    BREP_INFO("confirm_close_or_save: discard changes");
    return true;
  }

  // Save then close. Abort close if the user cancel the save dialog / fails.
  BREP_INFO("confirm_close_or_save: saving before close");
  const auto result = run_command("file.save");
  BREP_INFO("confirm_close_or_save: save status={}", int(result.status));
  return result.succeeded();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  BREP_INFO("MainWindow::closeEvent");
  if (!confirm_close_or_save()) {
    BREP_INFO("MainWindow::closeEvent ignored");
    event->ignore();
    return;
  }

  // Tear down app-wide hooks before child/Vulkan destruction churn.
  if (qApp) qApp->removeEventFilter(this);
  hide_cursor_tip();
  if (view_cube_) {
    view_cube_->hide();
    view_cube_->set_camera(nullptr);
  }
  for (auto* window : view_windows_) {
    if (!window) continue;
    window->set_selection_callback({});
    window->set_tool_motion_callback({});
    window->set_tool_press_callback({});
    window->set_context_menu_callback({});
    window->set_world(nullptr);
  }
  BREP_INFO("MainWindow::closeEvent accepted");
  event->accept();
}

void MainWindow::showEvent(QShowEvent* event) {
  QMainWindow::showEvent(event);
  if (view_cube_) {
    view_cube_->show();
    place_view_cube();
  }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  place_view_cube();
}

void MainWindow::moveEvent(QMoveEvent* event) {
  QMainWindow::moveEvent(event);
  place_view_cube();
}

void MainWindow::changeEvent(QEvent* event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslate_ui();
  }
  QMainWindow::changeEvent(event);
  if (!view_cube_) return;
  if (event->type() == QEvent::WindowStateChange) {
    if (isMinimized()) {
      view_cube_->hide();
    } else {
      view_cube_->show();
      place_view_cube();
    }
  }
}

}  // namespace brep::viewer
