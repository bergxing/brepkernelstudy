#pragma once

#include "commands/command_registry.hpp"
#include "commands/document_history.hpp"
#include "commands/itool.hpp"

#include <memory>
#include <string_view>

namespace brep::viewer::commands {

/// Runs instant commands and hosts at most one interactive tool (ESC cancels).
class CommandManager {
 public:
  explicit CommandManager(CommandRegistry& registry);

  [[nodiscard]] DocumentHistory& history() noexcept { return history_; }
  [[nodiscard]] const DocumentHistory& history() const noexcept {
    return history_;
  }

  [[nodiscard]] bool has_active_tool() const noexcept {
    return active_tool_ != nullptr;
  }
  [[nodiscard]] ITool* active_tool() noexcept { return active_tool_.get(); }
  [[nodiscard]] QString active_prompt() const;
  [[nodiscard]] bool active_tool_allows_selection() const noexcept;

  /// Instant → execute; Interactive → start tool (cancels previous tool).
  CommandResult run(std::string_view id, CommandContext& ctx);

  bool cancel_active_tool(CommandContext& ctx);

  /// Forward input to the active tool. Returns true if consumed.
  bool tool_mouse_press(CommandContext& ctx, float x, float y, int button);
  void tool_mouse_move(CommandContext& ctx, float x, float y);
  bool tool_key_press(CommandContext& ctx, int key);

 private:
  void finish_tool_if_done(CommandContext& ctx);

  CommandRegistry& registry_;
  DocumentHistory history_;
  std::unique_ptr<ITool> active_tool_;
  CommandContext tool_ctx_snapshot_{};  // kept for cancel/finish callbacks
};

}  // namespace brep::viewer::commands
