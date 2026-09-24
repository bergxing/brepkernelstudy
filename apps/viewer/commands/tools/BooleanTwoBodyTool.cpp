#include "commands/tools/BooleanTwoBodyTool.h"
#include "commands/tools/BooleanExecute.h"

#include "Camera.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"

#include <QAction>
#include <QCoreApplication>
#include <QCursor>
#include <QMenu>
#include <QMouseEvent>
#include <cstdint>
#include <string_view>
#include <vector>

namespace brep::viewer::commands
{
namespace
{

QString TrBoolTool(const char* source)
{
    return QCoreApplication::translate("BooleanTwoBodyTool", source);
}

[[nodiscard]] const char* OpLogName(boolean::BooleanOp op) noexcept
{
    switch (op)
    {
        case boolean::BooleanOp::Union:
            return "union";
        case boolean::BooleanOp::Subtract:
            return "subtract";
        case boolean::BooleanOp::Intersect:
            return "intersect";
    }
    return "boolean";
}

[[nodiscard]] QString BodyName(const entt::registry& registry,
                               entt::entity entity)
{
    if (entity == entt::null || !registry.valid(entity))
    {
        return QStringLiteral("?");
    }
    if (const auto* name = registry.try_get<ecs::Name>(entity))
    {
        if (!name->value.empty())
        {
            return QString::fromStdString(name->value);
        }
    }
    return QString::fromStdString(ecs::selection_label(registry, entity));
}

[[nodiscard]] QString BodyGuidString(const entt::registry& registry,
                                     entt::entity entity)
{
    if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
    {
        return QString::fromStdString(body->guid.ToString());
    }
    return QStringLiteral("-");
}

[[nodiscard]] QString FeatureGuidString(const entt::registry& registry,
                                        entt::entity entity)
{
    if (const auto* feature = registry.try_get<ecs::FeatureRef>(entity))
    {
        if (feature->FeatureGuid.IsValid())
        {
            return QString::fromStdString(feature->FeatureGuid.ToString());
        }
    }
    return QStringLiteral("-");
}

void LogPick(boolean::BooleanOp op, const entt::registry& registry,
             entt::entity entity, std::string_view role)
{
    BREP_INFO(
        "BooleanTwoBodyTool {} {} entity={} name='{}' body={} feature={}",
        OpLogName(op), role, static_cast<std::uint32_t>(entity),
        BodyName(registry, entity).toStdString(),
        BodyGuidString(registry, entity).toStdString(),
        FeatureGuidString(registry, entity).toStdString());
}

void LogRayHits(boolean::BooleanOp op, CommandContext& ctx, float x, float y,
                entt::entity skip)
{
    if (ctx.World == nullptr || ctx.ViewCamera == nullptr)
    {
        return;
    }
    auto& registry = ctx.World->registry();
    const std::vector<ecs::RayPickHit> hits = ecs::PickRenderablesAlongRay(
        registry, *ctx.ViewCamera, ctx.ViewportWidth, ctx.ViewportHeight, x,
        y);
    BREP_INFO("BooleanTwoBodyTool {} ray ({:.1f},{:.1f}) hits={} skip={}",
              OpLogName(op), x, y, hits.size(),
              skip == entt::null ? 0U : static_cast<std::uint32_t>(skip));
    for (std::size_t i = 0; i < hits.size(); ++i)
    {
        const entt::entity entity = hits[i].Entity;
        BREP_INFO(
            "BooleanTwoBodyTool {} ray[{}] t={:.4f} entity={} name='{}' "
            "body={} feature={}",
            OpLogName(op), i, hits[i].T, static_cast<std::uint32_t>(entity),
            BodyName(registry, entity).toStdString(),
            BodyGuidString(registry, entity).toStdString(),
            FeatureGuidString(registry, entity).toStdString());
    }
}

[[nodiscard]] entt::entity PickForBoolean(CommandContext& ctx, float x,
                                          float y, entt::entity skip)
{
    if (ctx.World == nullptr || ctx.ViewCamera == nullptr)
    {
        return entt::null;
    }
    return ecs::PickClosestRenderable(
        ctx.World->registry(), *ctx.ViewCamera, ctx.ViewportWidth,
        ctx.ViewportHeight, x, y, skip);
}

void Redraw(CommandContext& ctx)
{
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
    if (ctx.RefreshCursorTip)
    {
        ctx.RefreshCursorTip();
    }
}

void Report(CommandContext& ctx, const QString& message)
{
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(message);
    }
}

[[nodiscard]] bool EntityLive(const entt::registry& registry,
                              entt::entity entity)
{
    return entity != entt::null && registry.valid(entity);
}

}  // namespace

BooleanTwoBodyTool::BooleanTwoBodyTool(boolean::BooleanOp op)
    : m_op(op)
{
}

std::string_view BooleanTwoBodyTool::Id() const noexcept
{
    switch (m_op)
    {
        case boolean::BooleanOp::Union:
            return "boolean.union";
        case boolean::BooleanOp::Subtract:
            return "boolean.subtract";
        case boolean::BooleanOp::Intersect:
            return "boolean.intersect";
    }
    return "boolean.unknown";
}

QString BooleanTwoBodyTool::OpNoun() const
{
    switch (m_op)
    {
        case boolean::BooleanOp::Union:
            return TrBoolTool("Union");
        case boolean::BooleanOp::Subtract:
            return TrBoolTool("Subtract");
        case boolean::BooleanOp::Intersect:
            return TrBoolTool("Intersect");
    }
    return TrBoolTool("Boolean");
}

QString BooleanTwoBodyTool::HistoryLabel() const
{
    switch (m_op)
    {
        case boolean::BooleanOp::Union:
            return TrBoolTool("Boolean Union");
        case boolean::BooleanOp::Subtract:
            return TrBoolTool("Boolean Subtract");
        case boolean::BooleanOp::Intersect:
            return TrBoolTool("Boolean Intersect");
    }
    return TrBoolTool("Boolean");
}

QString BooleanTwoBodyTool::FailMessage(const QString& targetName,
                                        const QString& toolName) const
{
    switch (m_op)
    {
        case boolean::BooleanOp::Union:
            return TrBoolTool(
                       "Fuse %1 ∪ %2 failed. Pick the second body again, "
                       "or ESC and restart.")
                .arg(targetName, toolName);
        case boolean::BooleanOp::Subtract:
            return TrBoolTool(
                       "Cut %1 − %2 failed. Pick the tool again, "
                       "or ESC and restart with the body to keep.")
                .arg(targetName, toolName);
        case boolean::BooleanOp::Intersect:
            return TrBoolTool(
                       "Intersect %1 ∩ %2 failed. Pick the second body "
                       "again, or ESC and restart.")
                .arg(targetName, toolName);
    }
    return TrBoolTool("Boolean failed");
}

QString BooleanTwoBodyTool::Prompt() const
{
    switch (m_step)
    {
        case Step::SelectTarget:
            return TrBoolTool(
                       "Boolean %1: select the first object (target) "
                       "(ESC cancel)")
                .arg(OpNoun());
        case Step::SelectTool:
            if (HasBothOperands())
            {
                return TrBoolTool(
                           "Boolean %1: press Enter to confirm, or "
                           "right-click Confirm (ESC cancel)")
                    .arg(OpNoun());
            }
            return TrBoolTool(
                       "Boolean %1: select the second object "
                       "(hold Ctrl to multi-select), then press Enter "
                       "(ESC cancel)")
                .arg(OpNoun());
    }
    return {};
}

bool BooleanTwoBodyTool::HasBothOperands() const noexcept
{
    return m_target != entt::null && m_tool != entt::null &&
           m_target != m_tool;
}

void BooleanTwoBodyTool::ApplyTarget(CommandContext& ctx, entt::entity hit)
{
    ecs::set_selection(ctx.World->registry(), hit);
    m_target = hit;
    m_tool = entt::null;
    m_step = Step::SelectTool;
    const QString name = BodyName(ctx.World->registry(), hit);
    Report(ctx,
           TrBoolTool("Target: %1. Select the second object "
                      "(hold Ctrl to multi-select), then press Enter "
                      "(ESC cancel)")
               .arg(name));
    Redraw(ctx);
    LogPick(m_op, ctx.World->registry(), hit, "target");
}

void BooleanTwoBodyTool::ApplyTool(CommandContext& ctx, entt::entity hit)
{
    if (hit == m_target)
    {
        Report(ctx, TrBoolTool(
                        "Select a different object as the tool "
                        "(hold Ctrl to multi-select)"));
        return;
    }
    m_tool = hit;
    ecs::select_entities(ctx.World->registry(), {m_target, m_tool}, false);
    const QString targetName = BodyName(ctx.World->registry(), m_target);
    const QString toolName = BodyName(ctx.World->registry(), m_tool);
    Report(ctx, TrBoolTool("Target: %1, tool: %2. Press Enter to confirm, "
                           "or right-click Confirm (ESC cancel)")
                    .arg(targetName, toolName));
    Redraw(ctx);
    LogPick(m_op, ctx.World->registry(), hit, "tool");
}

bool BooleanTwoBodyTool::TryCommit(CommandContext& ctx)
{
    if (!HasBothOperands() || ctx.World == nullptr || ctx.Scene == nullptr)
    {
        Report(ctx, Prompt());
        return false;
    }
    auto& registry = ctx.World->registry();
    if (!EntityLive(registry, m_target) || !EntityLive(registry, m_tool))
    {
        m_target = entt::null;
        m_tool = entt::null;
        m_step = Step::SelectTarget;
        Report(ctx, Prompt());
        return false;
    }
    ecs::select_entities(registry, {m_target, m_tool}, false);
    const auto operands = ResolveBooleanOperands(registry, *ctx.Scene);
    if (!operands)
    {
        Report(ctx, TrBoolTool(
                        "Cannot resolve boolean operands (need two distinct "
                        "features)"));
        return false;
    }
    const QString targetName = BodyName(registry, m_target);
    const QString toolName = BodyName(registry, m_tool);
    m_result = ExecuteBoolean(ctx, m_op, operands->Target, operands->Tool,
                              HistoryLabel());
    if (!m_result.Succeeded())
    {
        m_finished = false;
        Report(ctx, FailMessage(targetName, toolName));
        return false;
    }
    m_finished = true;
    return true;
}

void BooleanTwoBodyTool::OnStart(CommandContext& ctx)
{
    m_finished = false;
    m_result = CommandResult::Cancelled();
    m_target = entt::null;
    m_tool = entt::null;
    m_step = Step::SelectTarget;

    if (ctx.World == nullptr)
    {
        m_result = CommandResult::Failed(TrBoolTool("Invalid context"));
        m_finished = true;
        return;
    }

    const std::vector<entt::entity> ordered =
        ecs::SelectedEntitiesOrdered(ctx.World->registry());
    if (ordered.size() >= 2U)
    {
        m_target = ordered[0];
        m_tool = ordered[1];
        m_step = Step::SelectTool;
        LogPick(m_op, ctx.World->registry(), m_target, "target(preselected)");
        LogPick(m_op, ctx.World->registry(), m_tool, "tool(preselected)");
        if (!TryCommit(ctx))
        {
            if (m_result.Status == CommandStatus::Cancelled)
            {
                m_result = CommandResult::Failed(
                    TrBoolTool("Cannot resolve boolean operands (need two "
                               "distinct features)"));
            }
            m_finished = true;
        }
        return;
    }
    if (ordered.size() == 1U)
    {
        m_target = ordered[0];
        m_step = Step::SelectTool;
        const QString name = BodyName(ctx.World->registry(), m_target);
        Report(ctx,
               TrBoolTool("Target: %1. Select the second object "
                          "(hold Ctrl to multi-select), then press Enter "
                          "(ESC cancel)")
                   .arg(name));
        BREP_INFO("BooleanTwoBodyTool {} start with target preselected '{}'",
                  OpLogName(m_op), name.toStdString());
        LogPick(m_op, ctx.World->registry(), m_target, "target(preselected)");
        return;
    }

    Report(ctx, Prompt());
    BREP_INFO("BooleanTwoBodyTool {} start (select target)",
              OpLogName(m_op));
}

bool BooleanTwoBodyTool::OnMousePress(CommandContext& ctx, float x, float y,
                                      int button)
{
    if (button != Qt::LeftButton || ctx.World == nullptr)
    {
        return false;
    }
    const entt::entity skip =
        m_step == Step::SelectTool ? m_target : entt::null;
    LogRayHits(m_op, ctx, x, y, skip);
    const entt::entity hit = PickForBoolean(ctx, x, y, skip);
    if (hit == entt::null)
    {
        Report(ctx, TrBoolTool("No object under cursor — pick a body"));
        return true;
    }
    if (m_step == Step::SelectTarget)
    {
        ApplyTarget(ctx, hit);
        return true;
    }
    ApplyTool(ctx, hit);
    return true;
}

void BooleanTwoBodyTool::OnMouseMove(CommandContext& ctx, float x, float y)
{
    if (ctx.World == nullptr || ctx.ViewCamera == nullptr)
    {
        return;
    }
    auto& registry = ctx.World->registry();
    const entt::entity skip =
        m_step == Step::SelectTool ? m_target : entt::null;
    const entt::entity hit = PickForBoolean(ctx, x, y, skip);
    const entt::entity prev = ecs::hovered_entity(registry);
    ecs::set_hover(registry, hit);
    if (hit != prev && ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
}

bool BooleanTwoBodyTool::OnKeyPress(CommandContext& ctx, int key)
{
    if (key != Qt::Key_Return && key != Qt::Key_Enter)
    {
        return false;
    }
    if (!HasBothOperands())
    {
        Report(ctx, Prompt());
        return true;
    }
    TryCommit(ctx);
    return true;
}

bool BooleanTwoBodyTool::OnContextMenu(CommandContext& ctx, float x, float y)
{
    (void)x;
    (void)y;
    QMenu menu(ctx.ParentWidget);
    menu.setCursor(Qt::ArrowCursor);
    QAction* actConfirm = menu.addAction(TrBoolTool("Confirm"));
    QAction* actCancel = menu.addAction(TrBoolTool("Cancel"));
    actConfirm->setEnabled(HasBothOperands());
    const QPoint menuPos = QCursor::pos();
    BREP_INFO("BooleanTwoBodyTool {} context menu at global=({},{})",
              OpLogName(m_op), menuPos.x(), menuPos.y());
    QAction* chosen = menu.exec(menuPos);
    if (m_finished)
    {
        return true;
    }
    if (chosen == actConfirm)
    {
        TryCommit(ctx);
        return true;
    }
    if (chosen == actCancel)
    {
        OnCancel(ctx);
        return true;
    }
    return true;
}

void BooleanTwoBodyTool::OnCancel(CommandContext& ctx)
{
    m_finished = true;
    m_result = CommandResult::Cancelled(
        TrBoolTool("Cancelled boolean %1").arg(OpNoun()));
    Report(ctx, m_result.Message);
}

}  // namespace brep::viewer::commands
