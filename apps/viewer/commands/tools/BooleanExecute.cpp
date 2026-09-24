#include "commands/tools/BooleanExecute.h"

#include "adapter/ISceneServiceFactory.h"
#include "commands/DocumentHistory.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"

#include "api/Core.h"

#include <QCoreApplication>

#include <vector>

namespace brep::viewer::commands
{
namespace
{

QString TrBool(const char* source)
{
    return QCoreApplication::translate("BooleanExecute", source);
}

[[nodiscard]] const char* ResultName(boolean::BooleanOp op) noexcept
{
    switch (op)
    {
        case boolean::BooleanOp::Union:
            return "Fuse";
        case boolean::BooleanOp::Subtract:
            return "Cut";
        case boolean::BooleanOp::Intersect:
            return "Common";
    }
    return "Boolean";
}

}  // namespace

std::optional<feat::FeatureId> FeatureIdForEntity(
    entt::registry& registry, adapter::ISceneService& scene,
    entt::entity entity)
{
    if (entity == entt::null)
    {
        return std::nullopt;
    }
    Guid featureGuid{};
    Guid bodyGuid{};
    if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity))
    {
        featureGuid = fref->FeatureGuid;
    }
    if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
    {
        bodyGuid = body->guid;
    }
    return scene.FeatureIdFor(featureGuid, bodyGuid);
}

std::optional<BooleanOperands> ResolveBooleanOperands(
    entt::registry& registry, adapter::ISceneService& scene)
{
    const std::vector<entt::entity> ordered =
        ecs::SelectedEntitiesOrdered(registry);
    if (ordered.size() < 2U)
    {
        return std::nullopt;
    }
    const auto target = FeatureIdForEntity(registry, scene, ordered[0]);
    const auto tool = FeatureIdForEntity(registry, scene, ordered[1]);
    if (!target || !tool || *target == *tool)
    {
        return std::nullopt;
    }
    return BooleanOperands{*target, *tool};
}

CommandResult ExecuteBoolean(CommandContext& ctx, boolean::BooleanOp op,
                             feat::FeatureId target, feat::FeatureId tool,
                             const QString& historyLabel)
{
    if (!ctx.Scene || !ctx.World)
    {
        return CommandResult::Failed(TrBool("Invalid context"));
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (part == nullptr)
    {
        return CommandResult::Failed(TrBool("No active Part"));
    }

    const auto* targetFeat = part->Features().Find(target);
    const auto* toolFeat = part->Features().Find(tool);
    BREP_INFO("boolean {} target='{}' ({}) tool='{}' ({})", ResultName(op),
              targetFeat != nullptr ? targetFeat->DisplayName() : "?",
              target.Guid.ToString(),
              toolFeat != nullptr ? toolFeat->DisplayName() : "?",
              tool.Guid.ToString());
    Body* result = scene.AddBoolean(op, target, tool, ResultName(op));
    if (result == nullptr)
    {
        const QString detail = QString::fromStdString(part->LastRegenError());
        return CommandResult::Failed(
            detail.isEmpty() ? TrBool("Boolean failed") : detail);
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    ctx.World->SyncPartBodies(*part, std::move(material));
    ecs::clear_selection(ctx.World->registry());
    if (ctx.Session)
    {
        ctx.Session->MarkDirty();
    }
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }

    const std::string wood = ctx.WoodAlbedoPath;
    ecs::World* world = ctx.World;
    Part* partPtr = part;
    if (ctx.History)
    {
        ctx.History->push({
            .label = historyLabel,
            .undo =
                [world, partPtr, wood, factory = ctx.SceneFactory,
                 session = ctx.Session, redraw = ctx.RequestRedraw,
                 refresh = ctx.RefreshUi]
            {
                if (world == nullptr || partPtr == nullptr)
                {
                    return;
                }
                adapter::WithScene(
                    factory, world->Document(),
                    [](adapter::ISceneService& scn) { scn.UndoFeature(); });
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->SyncPartBodies(*partPtr, std::move(mat));
                if (session)
                {
                    session->MarkDirty();
                }
                if (redraw)
                {
                    redraw();
                }
                if (refresh)
                {
                    refresh();
                }
            },
            .redo =
                [world, partPtr, wood, factory = ctx.SceneFactory,
                 session = ctx.Session, redraw = ctx.RequestRedraw,
                 refresh = ctx.RefreshUi]
            {
                if (world == nullptr || partPtr == nullptr)
                {
                    return;
                }
                adapter::WithScene(
                    factory, world->Document(),
                    [](adapter::ISceneService& scn) { scn.RedoFeature(); });
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->SyncPartBodies(*partPtr, std::move(mat));
                if (session)
                {
                    session->MarkDirty();
                }
                if (redraw)
                {
                    redraw();
                }
                if (refresh)
                {
                    refresh();
                }
            },
        });
    }

    if (ctx.RefreshUi)
    {
        ctx.RefreshUi();
    }
    const QString msg =
        TrBool("Boolean done: %1").arg(QString::fromStdString(result->Name));
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(msg);
    }
    return CommandResult::Ok(msg);
}

}  // namespace brep::viewer::commands
