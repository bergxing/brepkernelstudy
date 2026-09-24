#include "commands/tools/SelectedBodies.h"

#include "commands/CommandTypes.h"
#include "ecs/Components.h"

#include "adapter/ISceneService.h"

#include <cstddef>

namespace brep::viewer::commands
{

SelectedBodies CollectSelectedBodies(CommandContext& ctx)
{
    SelectedBodies out;
    if (!ctx.World || !ctx.World->Document() || !ctx.Scene)
    {
        return out;
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
        return out;
    }

    auto& registry = ctx.World->registry();
    auto view = registry.view<ecs::SelectedTag>();
    for (auto entity : view)
    {
        ++out.SelectedCount;
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
        if (!bodyGuid.IsValid() && featureGuid.IsValid())
        {
            if (const auto* f =
                    part->Features().Find(feat::FeatureId{featureGuid}))
            {
                bodyGuid = f->BodyGuid();
            }
        }
        SelectedBody item;
        item.BodyGuid = bodyGuid;
        item.BodyExists = bodyGuid.IsValid() && part->FindBody(bodyGuid);
        // Prefer body Guid so a stale FeatureRef cannot hide the Spec.
        item.Spec = scene.SpecFor(Guid{}, bodyGuid);
        if (!item.Spec.has_value() && featureGuid.IsValid())
        {
            item.Spec = scene.SpecFor(featureGuid, Guid{});
        }
        out.Items.push_back(std::move(item));
    }
    return out;
}

namespace
{

[[nodiscard]] bool CanCopy(const SelectedBody& item)
{
    return item.Spec.has_value() || item.BodyExists;
}

[[nodiscard]] bool CanMove(const SelectedBody& item)
{
    return item.Spec.has_value() && item.BodyExists;
}

}  // namespace

std::vector<SelectedBody> CopyableBodies(const SelectedBodies& selection)
{
    std::vector<SelectedBody> out;
    for (const auto& item : selection.Items)
    {
        if (CanCopy(item))
        {
            out.push_back(item);
        }
    }
    return out;
}

std::vector<Guid> MovableBodyGuids(const SelectedBodies& selection)
{
    std::vector<Guid> out;
    for (const auto& item : selection.Items)
    {
        if (CanMove(item))
        {
            out.push_back(item.BodyGuid);
        }
    }
    return out;
}

bool HasCopyable(const SelectedBodies& selection)
{
    return !CopyableBodies(selection).empty();
}

bool AllCopyable(const SelectedBodies& selection)
{
    return selection.SelectedCount > 0 &&
           CopyableBodies(selection).size() ==
               static_cast<std::size_t>(selection.SelectedCount);
}

bool AllMovable(const SelectedBodies& selection)
{
    return selection.SelectedCount > 0 &&
           MovableBodyGuids(selection).size() ==
               static_cast<std::size_t>(selection.SelectedCount);
}

}  // namespace brep::viewer::commands
