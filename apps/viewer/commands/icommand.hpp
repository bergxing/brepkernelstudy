#pragma once

#include "commands/command_types.hpp"
#include "commands/itool.hpp"

#include <memory>
#include <string_view>

namespace brep::viewer::commands {

class ICommand {
 public:
  virtual ~ICommand() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view title() const noexcept = 0;
  [[nodiscard]] virtual CommandKind kind() const noexcept {
    return CommandKind::Instant;
  }

  [[nodiscard]] virtual bool can_execute(const CommandContext& ctx) const {
    return ctx.world != nullptr;
  }

  virtual CommandResult execute(CommandContext& ctx) {
    (void)ctx;
    return CommandResult::failed(QStringLiteral("命令未实现 execute()"));
  }

  [[nodiscard]] virtual std::unique_ptr<ITool> make_tool(
      CommandContext& ctx) const {
    (void)ctx;
    return nullptr;
  }
};

}  // namespace brep::viewer::commands
