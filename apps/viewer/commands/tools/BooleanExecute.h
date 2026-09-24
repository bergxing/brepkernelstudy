#pragma once

#include "commands/CommandTypes.h"

#include "api/Modeling.h"

#include <entt/entt.hpp>

#include <optional>

namespace brep::viewer::commands
{

struct BooleanOperands
{
    feat::FeatureId Target{};
    feat::FeatureId Tool{};
};

[[nodiscard]] std::optional<feat::FeatureId> FeatureIdForEntity(
    entt::registry& registry, adapter::ISceneService& scene,
    entt::entity entity);

/// First selected body is the target; second is the tool.
[[nodiscard]] std::optional<BooleanOperands> ResolveBooleanOperands(
    entt::registry& registry, adapter::ISceneService& scene);

[[nodiscard]] CommandResult ExecuteBoolean(CommandContext& ctx,
                                           boolean::BooleanOp op,
                                           feat::FeatureId target,
                                           feat::FeatureId tool,
                                           const QString& historyLabel);

}  // namespace brep::viewer::commands
