#include "MainWindow.h"

#include "commands/CommandManager.h"
#include "commands/snap/SnapOverlay.h"

#include <QCursor>
#include <QLabel>
#include <QMdiSubWindow>
#include <QRect>

#include <algorithm>

namespace brep::viewer
{

void MainWindow::setup_cursor_tip()
{
  m_cursorTip = new QLabel(this);
  m_cursorTip->setObjectName(QStringLiteral("CursorTip"));
  m_cursorTip->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_cursorTip->setAttribute(Qt::WA_ShowWithoutActivating);
  m_cursorTip->setFocusPolicy(Qt::NoFocus);
  m_cursorTip->setStyleSheet(QStringLiteral(
      "QLabel#CursorTip {"
      "  background-color: rgba(28, 28, 28, 210);"
      "  color: #f2f2f2;"
      "  border: 1px solid rgba(255, 255, 255, 45);"
      "  border-radius: 3px;"
      "  padding: 3px 8px;"
      "  font-size: 12px;"
      "}"));
  m_cursorTip->hide();
}

void MainWindow::hide_cursor_tip()
{
  if (m_cursorTip) m_cursorTip->hide();
}

bool MainWindow::map_global_to_viewport(const QPoint& global,
                                        VulkanWindow*& out_window, float& out_x,
                                        float& out_y) const
                                        {
  out_window = nullptr;
  auto* vw = vulkan_window_at_global(global);
  if (!vw) return false;

  QWidget* container = nullptr;
  for (auto it = m_viewWindows.constBegin(); it != m_viewWindows.constEnd();
       ++it)
       {
    if (it.value() == vw)
       {
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
                                            float y)
{
  (void)window;
  (void)x;
  (void)y;

  if (m_commandManager.has_active_tool())
  {
    QString prompt = m_commandManager.active_prompt();
    prompt.remove(QStringLiteral(" (ESC 取消)"));
    if (m_snapSession.active_snap)
    {
      const QString snap_name =
          commands::SnapKindName(*m_snapSession.active_snap);
      if (!snap_name.isEmpty())
      {
        if (!prompt.isEmpty()) prompt += QLatin1Char('\n');
        prompt += snap_name;
      }
    }
    if (!prompt.isEmpty()) return prompt;
  }

  return {};
}

void MainWindow::update_cursor_tip_at_global(const QPoint& global)
{
  if (!m_cursorTip) return;

  if (m_viewCube && m_viewCube->isVisible())
  {
    const QRect cube_global(m_viewCube->pos(), m_viewCube->size());
    if (cube_global.contains(global))
    {
      hide_cursor_tip();
      return;
    }
  }

  VulkanWindow* vw = nullptr;
  float x = 0.0f;
  float y = 0.0f;
  if (!map_global_to_viewport(global, vw, x, y))
  {
    hide_cursor_tip();
    return;
  }

  const QString text = resolve_cursor_tip_text(vw, x, y);
  if (text.isEmpty())
  {
    hide_cursor_tip();
    return;
  }

  m_cursorTip->setText(text);
  m_cursorTip->adjustSize();

  constexpr int kOffset = 16;
  QPoint pos = mapFromGlobal(global + QPoint(kOffset, kOffset));
  const QRect bounds = rect().adjusted(4, 4, -4, -4);
  const int max_x =
      std::max(bounds.left(), bounds.right() - m_cursorTip->width());
  const int max_y =
      std::max(bounds.top(), bounds.bottom() - m_cursorTip->height());
  pos.setX(std::clamp(pos.x(), bounds.left(), max_x));
  pos.setY(std::clamp(pos.y(), bounds.top(), max_y));

  m_cursorTip->move(pos);
  m_cursorTip->show();
  m_cursorTip->raise();
}

void MainWindow::refresh_cursor_tip()
{
  update_cursor_tip_at_global(QCursor::pos());
}

}  // namespace brep::viewer
