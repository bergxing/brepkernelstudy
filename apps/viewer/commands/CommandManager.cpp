#include "commands/CommandManager.h"

#include "commands/CommandPayload.h"

#include "api/Core.h"

#include <QApplication>
#include <QWidget>

namespace brep::viewer::commands
{
namespace
{

void CloseActivePopupWidget()
{
    if (QWidget* popup = QApplication::activePopupWidget())
    {
        BREP_INFO("close popup '{}'", popup->metaObject()->className());
        popup->close();
    }
}

}  // namespace

CommandManager::CommandManager(CommandRegistry& registry, brep::AspectChain chain)
    : m_registry(registry)
    , m_chain(std::move(chain))
{
}

std::vector<std::string> CommandManager::Names() const
{
    return m_chain.Names();
}

QString CommandManager::active_prompt() const
{
    if (!m_activeTool)
    {
        return {};
    }
    return m_activeTool->Prompt();
}

bool CommandManager::active_tool_allows_selection() const noexcept
{
    return m_activeTool && m_activeTool->AllowsViewportSelection();
}

CommandResult CommandManager::run(std::string_view id, CommandContext& ctx)
{
    ctx.History = &m_history;
    if (m_activeTool && m_activeTool->OwnsUndoRedo())
    {
        if (id == "edit.undo")
        {
            if (!m_activeTool->UndoStep(ctx))
            {
                return CommandResult::Failed(
                    QStringLiteral("\u65e0\u53ef\u64a4\u9500\u6b65\u9aa4"));
            }
            finish_tool_if_done(ctx);
            return CommandResult::Ok();
        }
        if (id == "edit.redo")
        {
            if (!m_activeTool->RedoStep(ctx))
            {
                return CommandResult::Failed(
                    QStringLiteral("\u65e0\u53ef\u91cd\u505a\u6b65\u9aa4"));
            }
            finish_tool_if_done(ctx);
            return CommandResult::Ok();
        }
    }
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
        if (m_toolDispatchDepth > 0)
        {
            return CommandResult::Ok();
        }
        m_activeTool = cmd->make_tool(ctx);
        if (!m_activeTool)
        {
            return CommandResult::Failed(
                QStringLiteral("\u4ea4\u4e92\u547d\u4ee4\u672a\u63d0\u4f9b Tool"));
        }
        m_toolCtxSnapshot = ctx;
        m_toolCtxSnapshot.History = &m_history;
        brep::AspectEvent startEvent;
        startEvent.Site = "command.tool.start";
        startEvent.Subject = m_activeTool->Id();
        CommandPayload startPayload;
        startPayload.ReportStatus = m_toolCtxSnapshot.ReportStatus
                                        ? &m_toolCtxSnapshot.ReportStatus
                                        : nullptr;
        startEvent.Payload = &startPayload;
        m_chain.Invoke(startEvent, [&] {
            m_activeTool->OnStart(m_toolCtxSnapshot);
            if (!m_activeTool->IsFinished())
            {
                startPayload.Prompt = m_activeTool->Prompt();
            }
        });
        if (m_activeTool->IsFinished())
        {
            const CommandResult r = m_activeTool->Result();
            finish_tool_if_done(m_toolCtxSnapshot);
            return r;
        }
        return CommandResult::Ok(m_activeTool->Prompt());
    }

    brep::AspectEvent event;
    event.Site = "command.run";
    event.Subject = cmd->id();
    CommandPayload payload;
    payload.ReportStatus = ctx.ReportStatus ? &ctx.ReportStatus : nullptr;
    event.Payload = &payload;
    return m_chain.Invoke(event, [&] {
        payload.ResultStorage = cmd->execute(ctx);
        payload.Result = &payload.ResultStorage;
        payload.DetailUtf8 = payload.ResultStorage.Message.toUtf8();
        event.Detail = payload.DetailUtf8.constData();
        event.Failed = payload.ResultStorage.Status == CommandStatus::Failed;
        return payload.ResultStorage;
    });
}

bool CommandManager::cancel_active_tool(CommandContext& ctx)
{
    if (!m_activeTool)
    {
        return false;
    }
    CloseActivePopupWidget();
    brep::AspectEvent event;
    event.Site = "command.tool.cancel";
    event.Subject = m_activeTool->Id();
    CommandPayload payload;
    payload.ReportStatus = ctx.ReportStatus ? &ctx.ReportStatus : nullptr;
    event.Payload = &payload;
    m_chain.Invoke(event, [&] {
        m_activeTool->OnCancel(ctx);
        payload.AnnounceCancel = m_activeTool && !m_activeTool->IsFinished() &&
                                 m_toolDispatchDepth == 0;
    });
    if (m_activeTool && m_activeTool->IsFinished())
    {
        finish_tool_if_done(ctx);
        return true;
    }
    if (m_toolDispatchDepth > 0)
    {
        return true;
    }
    m_activeTool.reset();
    return true;
}

void CommandManager::finish_tool_if_done(CommandContext& ctx)
{
    if (!m_activeTool || !m_activeTool->IsFinished())
    {
        return;
    }
    if (m_toolDispatchDepth > 0)
    {
        return;
    }
    brep::AspectEvent event;
    event.Site = "command.tool.finish";
    event.Subject = m_activeTool->Id();
    CommandPayload payload;
    payload.ReportStatus = ctx.ReportStatus ? &ctx.ReportStatus : nullptr;
    event.Payload = &payload;
    m_chain.Invoke(event, [&] {
        payload.ResultStorage = m_activeTool->Result();
        payload.Result = &payload.ResultStorage;
        payload.DetailUtf8 = payload.ResultStorage.Message.toUtf8();
        event.Detail = payload.DetailUtf8.constData();
        event.Failed = payload.ResultStorage.Status == CommandStatus::Failed;
    });
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
    ++m_toolDispatchDepth;
    const bool consumed = m_activeTool->OnMousePress(ctx, x, y, button);
    --m_toolDispatchDepth;
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
    ++m_toolDispatchDepth;
    m_activeTool->OnMouseMove(ctx, x, y);
    --m_toolDispatchDepth;
    finish_tool_if_done(ctx);
}

bool CommandManager::tool_key_press(CommandContext& ctx, int key)
{
    if (!m_activeTool)
    {
        return false;
    }
    ctx.History = &m_history;
    ++m_toolDispatchDepth;
    const bool consumed = m_activeTool->OnKeyPress(ctx, key);
    --m_toolDispatchDepth;
    finish_tool_if_done(ctx);
    return consumed;
}

bool CommandManager::tool_context_menu(CommandContext& ctx, float x, float y)
{
    if (!m_activeTool)
    {
        return false;
    }
    ctx.History = &m_history;
    BREP_INFO("tool context menu '{}' at ({:.1f},{:.1f})", m_activeTool->Id(),
              x, y);
    ++m_toolDispatchDepth;
    const bool consumed = m_activeTool->OnContextMenu(ctx, x, y);
    --m_toolDispatchDepth;
    finish_tool_if_done(ctx);
    return consumed;
}

}  // namespace brep::viewer::commands
