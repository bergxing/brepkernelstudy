#include "commands/command_registry.hpp"

#include "api/core.hpp"

#include <algorithm>

namespace brep::viewer::commands {

void CommandRegistry::register_command(std::string id, Factory factory) {
  if (id.empty() || !factory) {
    BREP_WARN("CommandRegistry: refusing empty id or null factory");
    return;
  }
  if (factories_.contains(id)) {
    BREP_WARN("CommandRegistry: replacing command '{}'", id);
  }
  factories_[std::move(id)] = std::move(factory);
}

bool CommandRegistry::contains(std::string_view id) const {
  return factories_.contains(std::string(id));
}

std::unique_ptr<ICommand> CommandRegistry::create(std::string_view id) const {
  const auto it = factories_.find(std::string(id));
  if (it == factories_.end()) return nullptr;
  return it->second();
}

CommandResult CommandRegistry::execute(std::string_view id,
                                       CommandContext& ctx) const {
  auto cmd = create(id);
  if (!cmd) {
    BREP_ERROR("CommandRegistry: unknown command '{}'", id);
    return CommandResult::failed(
        QStringLiteral("未知命令: %1").arg(QString::fromStdString(std::string(id))));
  }
  if (!cmd->can_execute(ctx)) {
    return CommandResult::failed(
        QStringLiteral("命令当前不可执行: %1")
            .arg(QString::fromStdString(std::string(cmd->id()))));
  }

  BREP_INFO("command execute '{}'", cmd->id());
  CommandResult result = cmd->execute(ctx);
  if (result.status == CommandStatus::Failed) {
    BREP_ERROR("command '{}' failed: {}", cmd->id(),
               result.message.toStdString());
  }
  return result;
}

std::vector<std::string> CommandRegistry::ids() const {
  std::vector<std::string> out;
  out.reserve(factories_.size());
  for (const auto& [id, _] : factories_) out.push_back(id);
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace brep::viewer::commands
