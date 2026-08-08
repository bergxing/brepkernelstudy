#include "main_window.hpp"

#include "commands/command_manager.hpp"

#include <QCursor>
#include <QLabel>
#include <QMdiSubWindow>
#include <QRect>

#include <algorithm>

namespace brep::viewer {

void MainWindow::setup_cursor_tip() {
  cursor_tip_ = new QLabel(this);
  cursor_tip_->setObjectName(QStringLiteral("CursorTip"));
  cursor_tip_->setAttribute(Qt::WA_TransparentForMouseEvents);
  cursor_tip_->setAttribute(Qt::WA_ShowWithoutActivating);
  cursor_tip_->setFocusPolicy(Qt::NoFocus);
  cursor_tip_->setStyleSheet(QStringLiteral(
      "QLabel#CursorTip {"
      "  background-color: rgba(28, 28, 28, 210);"
      "  color: #f2f2f2;"
      "  border: 1px solid rgba(255, 255, 255, 45);"
      "  border-radius: 3px;"
      "  padding: 3px 8px;"
      "  font-size: 12px;"
      "}"));
  cursor_tip_->hide();
}

void MainWindow::hide_cursor_tip() {
  if (cursor_tip_) cursor_tip_->hide();
}

bool MainWindow::map_global_to_viewport(const QPoint& global,
                                        VulkanWindow*& out_window, float& out_x,
                                        float& out_y) const {
  out_window = nullptr;
  auto* vw = vulkan_window_at_global(global);
  if (!vw) return false;

  QWidget* container = nullptr;
  for (auto it = view_windows_.constBegin(); it != view_windows_.constEnd();
       ++it) {
    if (it.value() == vw) {
      container = it.key() ? it.key()->widget() : nullptr;
      break;
    }
  }
  if (!container) return false;

  const QPoint local = container->mapFromGlobal(global);
  if (!container->rect().contains(local)) return false;

  const int cw = std::max(1, container->width());
  const int ch = std::max(1, container->height());
  const int vww = std::max(1, vw->width());
  const int vwh = std::max(1, vw->height());
  out_window = vw;
  out_x = float(local.x()) * float(vww) / float(cw);
  out_y = float(local.y()) * float(vwh) / float(ch);
  return true;
}

QString MainWindow::resolve_cursor_tip_text(VulkanWindow* window, float x,
                                            float y) {
  (void)window;
  (void)x;
  (void)y;

  if (command_manager_.has_active_tool()) {
    QString prompt = command_manager_.active_prompt();
    prompt.remove(QStringLiteral(" (ESC 取消)"));
    if (!prompt.isEmpty()) return prompt;
  }

  return {};
}

void MainWindow::update_cursor_tip_at_global(const QPoint& global) {
  if (!cursor_tip_) return;

  if (view_cube_ && view_cube_->isVisible()) {
    const QRect cube_global(view_cube_->pos(), view_cube_->size());
    if (cube_global.contains(global)) {
      hide_cursor_tip();
      return;
    }
  }

  VulkanWindow* vw = nullptr;
  float x = 0.0f;
  float y = 0.0f;
  if (!map_global_to_viewport(global, vw, x, y)) {
    hide_cursor_tip();
    return;
  }

  const QString text = resolve_cursor_tip_text(vw, x, y);
  if (text.isEmpty()) {
    hide_cursor_tip();
    return;
  }

  cursor_tip_->setText(text);
  cursor_tip_->adjustSize();

  constexpr int kOffset = 16;
  QPoint pos = mapFromGlobal(global + QPoint(kOffset, kOffset));
  const QRect bounds = rect().adjusted(4, 4, -4, -4);
  const int max_x =
      std::max(bounds.left(), bounds.right() - cursor_tip_->width());
  const int max_y =
      std::max(bounds.top(), bounds.bottom() - cursor_tip_->height());
  pos.setX(std::clamp(pos.x(), bounds.left(), max_x));
  pos.setY(std::clamp(pos.y(), bounds.top(), max_y));

  cursor_tip_->move(pos);
  cursor_tip_->show();
  cursor_tip_->raise();
}

void MainWindow::refresh_cursor_tip() {
  update_cursor_tip_at_global(QCursor::pos());
}

}  // namespace brep::viewer
