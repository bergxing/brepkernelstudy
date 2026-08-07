#pragma once

#include "commands/icommand.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace brep::viewer::commands {

/// Maps command id → factory. Actions only know the id; registry creates & runs.
class CommandRegistry {
 public:
  using Factory = std::function<std::unique_ptr<ICommand>()>;

  void register_command(std::string id, Factory factory);

  [[nodiscard]] bool contains(std::string_view id) const;
  [[nodiscard]] std::unique_ptr<ICommand> create(std::string_view id) const;

  /// create + can_execute + execute. Returns Failed if id unknown.
  CommandResult execute(std::string_view id, CommandContext& ctx) const;

  [[nodiscard]] std::vector<std::string> ids() const;

 private:
  std::unordered_map<std::string, Factory> factories_;
};

/// Register Phase-1 builtins: doc.new, file.export_dxf, part.create_box.
void register_builtin_commands(CommandRegistry& registry);

}  // namespace brep::viewer::commands
