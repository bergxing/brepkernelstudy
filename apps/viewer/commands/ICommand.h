#pragma once

#include "commands/CommandTypes.h"
#include "commands/ITool.h"

#include <memory>
#include <string_view>

namespace brep::viewer::commands
{

class ICommand
{
 public:
  virtual ~ICommand() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view title() const noexcept = 0;
  [[nodiscard]] virtual CommandKind kind() const noexcept
  {
    return CommandKind::Instant;
  }

  [[nodiscard]] virtual bool can_execute(const CommandContext& ctx) const
  {
    return ctx.World != nullptr;
  }

  virtual CommandResult execute(CommandContext& ctx)
  {
    (void)ctx;
    return CommandResult::Failed(
        QStringLiteral("\u547d\u4ee4\u672a\u5b9e\u73b0 execute()"));
  }

  [[nodiscard]] virtual std::unique_ptr<ITool> make_tool(
      CommandContext& ctx) const
  {
    (void)ctx;
    return nullptr;
  }
};

}  // namespace brep::viewer::commands
