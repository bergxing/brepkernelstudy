#include "commands/CommandRegistry.h"

#include "api/Core.h"

#include <algorithm>

namespace brep::viewer::commands
{

void CommandRegistry::register_command(std::string id, Factory factory)
{
  if (id.empty() || !factory)
{
    BREP_WARN("CommandRegistry: refusing empty id or null factory");
    return;
  }
  if (m_factories.contains(id))
  {
    BREP_WARN("CommandRegistry: replacing command '{}'", id);
  }
  m_factories[std::move(id)] = std::move(factory);
}

bool CommandRegistry::contains(std::string_view id) const
{
  return m_factories.contains(std::string(id));
}

std::unique_ptr<ICommand> CommandRegistry::create(std::string_view id) const
{
  const auto it = m_factories.find(std::string(id));
  if (it == m_factories.end()) return nullptr;
  return it->second();
}

CommandResult CommandRegistry::execute(std::string_view id,
                                       CommandContext& ctx) const
{
  auto cmd = create(id);
  if (!cmd)
  {
    BREP_ERROR("CommandRegistry: unknown command '{}'", id);
    return CommandResult::Failed(
        QStringLiteral("未知命令: %1").arg(QString::fromStdString(std::string(id))));
  }
  if (!cmd->can_execute(ctx))
  {
    return CommandResult::Failed(
        QStringLiteral("命令当前不可执行: %1")
            .arg(QString::fromStdString(std::string(cmd->id()))));
  }

  BREP_INFO("command execute '{}'", cmd->id());
  CommandResult result = cmd->execute(ctx);
  if (result.Status == CommandStatus::Failed)
  {
    BREP_ERROR("command '{}' failed: {}", cmd->id(),
               result.Message.toStdString());
  }
  return result;
}

std::vector<std::string> CommandRegistry::ids() const
{
  std::vector<std::string> out;
  out.reserve(m_factories.size());
  for (const auto& [id, _] : m_factories) out.push_back(id);
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace brep::viewer::commands
