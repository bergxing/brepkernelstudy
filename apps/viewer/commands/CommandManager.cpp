#include "commands/CommandManager.h"

#include "api/Core.h"

namespace brep::viewer::commands
{

CommandManager::CommandManager(CommandRegistry& registry)
    : m_registry(registry)
{
}

QString CommandManager::active_prompt() const
{
    if (!m_activeTool)
    {
        return {};
    }
    return m_activeTool->prompt();
}

bool CommandManager::active_tool_allows_selection() const noexcept
{
    return m_activeTool && m_activeTool->allows_viewport_selection();
}

CommandResult CommandManager::run(std::string_view id, CommandContext& ctx)
{
    ctx.History = &m_history;
    auto cmd = m_registry.create(id);
    if (!cmd)
    {
        return CommandResult::Failed(
            QStringLiteral("\u672a\u77e5\u547d\u4ee4: %1")
                .arg(QString::fromStdString(std::string(id))));
    }
    if (!cmd->can_execute(ctx))
    {
        return CommandResult::Failed(
            QStringLiteral("\u547d\u4ee4\u5f53\u524d\u4e0d\u53ef\u6267\u884c"));
    }

    if (cmd->kind() == CommandKind::Interactive)
    {
        cancel_active_tool(ctx);
        m_activeTool = cmd->make_tool(ctx);
        if (!m_activeTool)
        {
            return CommandResult::Failed(
                QStringLiteral("\u4ea4\u4e92\u547d\u4ee4\u672a\u63d0\u4f9b Tool"));
        }
        m_toolCtxSnapshot = ctx;
        m_toolCtxSnapshot.History = &m_history;
        m_activeTool->on_start(m_toolCtxSnapshot);
        if (m_activeTool->is_finished())
        {
            const CommandResult r = m_activeTool->result();
            BREP_INFO("tool finished immediately '{}' status={}",
                      m_activeTool->id(), int(r.Status));
            m_activeTool.reset();
            if (ctx.ReportStatus && !r.Message.isEmpty())
            {
                ctx.ReportStatus(r.Message);
            }
            return r;
        }
        const QString msg = m_activeTool->prompt();
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(msg);
        }
        BREP_INFO("tool started '{}'", m_activeTool->id());
        return CommandResult::Ok(msg);
    }

    // Instant (and View) commands.
    BREP_INFO("command execute '{}'", cmd->id());
    return cmd->execute(ctx);
}

bool CommandManager::cancel_active_tool(CommandContext& ctx)
{
    if (!m_activeTool)
    {
        return false;
    }
    BREP_INFO("tool cancel '{}'", m_activeTool->id());
    m_activeTool->on_cancel(ctx);
    const QString msg =
        QStringLiteral("\u5df2\u53d6\u6d88: %1")
            .arg(QString::fromStdString(std::string(m_activeTool->id())));
    m_activeTool.reset();
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(msg);
    }
    return true;
}

void CommandManager::finish_tool_if_done(CommandContext& ctx)
{
    if (!m_activeTool || !m_activeTool->is_finished())
    {
        return;
    }
    const CommandResult r = m_activeTool->result();
    BREP_INFO("tool finished '{}' status={}", m_activeTool->id(), int(r.Status));
    if (ctx.ReportStatus && !r.Message.isEmpty())
    {
        ctx.ReportStatus(r.Message);
    }
    m_activeTool.reset();
}

bool CommandManager::tool_mouse_press(CommandContext& ctx, float x, float y,
                                      int button)
{
    if (!m_activeTool)
    {
        return false;
    }
    ctx.History = &m_history;
    const bool consumed = m_activeTool->on_mouse_press(ctx, x, y, button);
    finish_tool_if_done(ctx);
    return consumed;
}

void CommandManager::tool_mouse_move(CommandContext& ctx, float x, float y)
{
    if (!m_activeTool)
    {
        return;
    }
    ctx.History = &m_history;
    m_activeTool->on_mouse_move(ctx, x, y);
}

bool CommandManager::tool_key_press(CommandContext& ctx, int key)
{
    if (!m_activeTool)
    {
        return false;
    }
    ctx.History = &m_history;
    const bool consumed = m_activeTool->on_key_press(ctx, key);
    finish_tool_if_done(ctx);
    return consumed;
}

}  // namespace brep::viewer::commands
