#pragma once

#include "commands/CommandRegistry.h"
#include "commands/DocumentHistory.h"
#include "commands/ITool.h"

#include <memory>
#include <string_view>

namespace brep::viewer::commands
{

/// Runs instant commands and hosts at most one interactive tool (ESC cancels).
class CommandManager
{
 public:
  explicit CommandManager(CommandRegistry& registry);

  [[nodiscard]] DocumentHistory& history() noexcept
  {
      return m_history; 
  }
  [[nodiscard]] const DocumentHistory& history() const noexcept
  {
    return m_history;
  }

  [[nodiscard]] bool has_active_tool() const noexcept
  {
    return m_activeTool != nullptr;
  }
  [[nodiscard]] ITool* active_tool() noexcept
  {
      return m_activeTool.get(); 
  }
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

  CommandRegistry& m_registry;
  DocumentHistory m_history;
  std::unique_ptr<ITool> m_activeTool;
  CommandContext m_toolCtxSnapshot{};  // kept for cancel/finish callbacks
};

}  // namespace brep::viewer::commands
