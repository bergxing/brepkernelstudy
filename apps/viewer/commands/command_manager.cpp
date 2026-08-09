#include "commands/command_manager.hpp"

#include "api/core.hpp"

namespace brep::viewer::commands {

CommandManager::CommandManager(CommandRegistry& registry)
    : registry_(registry) {}

QString CommandManager::active_prompt() const {
  if (!active_tool_) return {};
  return active_tool_->prompt();
}

bool CommandManager::active_tool_allows_selection() const noexcept {
  return active_tool_ && active_tool_->allows_viewport_selection();
}

CommandResult CommandManager::run(std::string_view id, CommandContext& ctx) {
  ctx.history = &history_;
  auto cmd = registry_.create(id);
  if (!cmd) {
    return CommandResult::failed(
        QStringLiteral("未知命令: %1")
            .arg(QString::fromStdString(std::string(id))));
  }
  if (!cmd->can_execute(ctx)) {
    return CommandResult::failed(QStringLiteral("命令当前不可执行"));
  }

  if (cmd->kind() == CommandKind::Interactive) {
    cancel_active_tool(ctx);
    active_tool_ = cmd->make_tool(ctx);
    if (!active_tool_) {
      return CommandResult::failed(QStringLiteral("交互命令未提供 Tool"));
    }
    tool_ctx_snapshot_ = ctx;
    tool_ctx_snapshot_.history = &history_;
    active_tool_->on_start(tool_ctx_snapshot_);
    if (active_tool_->is_finished()) {
      const CommandResult r = active_tool_->result();
      BREP_INFO("tool finished immediately '{}' status={}", active_tool_->id(),
                int(r.status));
      active_tool_.reset();
      if (ctx.report_status && !r.message.isEmpty()) {
        ctx.report_status(r.message);
      }
      return r;
    }
    const QString msg = active_tool_->prompt();
    if (ctx.report_status) ctx.report_status(msg);
    BREP_INFO("tool started '{}'", active_tool_->id());
    return CommandResult::ok(msg);
  }

  // Instant (and View) commands.
  BREP_INFO("command execute '{}'", cmd->id());
  return cmd->execute(ctx);
}

bool CommandManager::cancel_active_tool(CommandContext& ctx) {
  if (!active_tool_) return false;
  BREP_INFO("tool cancel '{}'", active_tool_->id());
  active_tool_->on_cancel(ctx);
  const QString msg = QStringLiteral("已取消: %1")
                          .arg(QString::fromStdString(std::string(active_tool_->id())));
  active_tool_.reset();
  if (ctx.report_status) ctx.report_status(msg);
  return true;
}

void CommandManager::finish_tool_if_done(CommandContext& ctx) {
  if (!active_tool_ || !active_tool_->is_finished()) return;
  const CommandResult r = active_tool_->result();
  BREP_INFO("tool finished '{}' status={}", active_tool_->id(),
            int(r.status));
  if (ctx.report_status && !r.message.isEmpty()) {
    ctx.report_status(r.message);
  }
  active_tool_.reset();
}

bool CommandManager::tool_mouse_press(CommandContext& ctx, float x, float y,
                                      int button) {
  if (!active_tool_) return false;
  ctx.history = &history_;
  const bool consumed = active_tool_->on_mouse_press(ctx, x, y, button);
  finish_tool_if_done(ctx);
  return consumed;
}

void CommandManager::tool_mouse_move(CommandContext& ctx, float x, float y) {
  if (!active_tool_) return;
  ctx.history = &history_;
  active_tool_->on_mouse_move(ctx, x, y);
}

bool CommandManager::tool_key_press(CommandContext& ctx, int key) {
  if (!active_tool_) return false;
  ctx.history = &history_;
  const bool consumed = active_tool_->on_key_press(ctx, key);
  finish_tool_if_done(ctx);
  return consumed;
}

}  // namespace brep::viewer::commands
