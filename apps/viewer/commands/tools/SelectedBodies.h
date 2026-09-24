#pragma once

#include "api/Core.h"
#include "api/Modeling.h"

#include <optional>
#include <vector>

namespace brep::viewer::commands
{

struct CommandContext;

/// One currently selected scene object. Spec is set when the body is a
/// reconstructable primitive (Box / Sphere / …).
struct SelectedBody
{
    Guid BodyGuid{};
    std::optional<PrimitiveSpec> Spec{};
    bool BodyExists{false};
};

struct SelectedBodies
{
    std::vector<SelectedBody> Items;
    int SelectedCount{0};
};

/// Shared pick list for Copy / Move / later Array. Tools only filter.
[[nodiscard]] SelectedBodies CollectSelectedBodies(CommandContext& ctx);

[[nodiscard]] std::vector<SelectedBody> CopyableBodies(
    const SelectedBodies& selection);

[[nodiscard]] std::vector<Guid> MovableBodyGuids(
    const SelectedBodies& selection);

[[nodiscard]] bool HasCopyable(const SelectedBodies& selection);
[[nodiscard]] bool AllCopyable(const SelectedBodies& selection);
[[nodiscard]] bool AllMovable(const SelectedBodies& selection);

}  // namespace brep::viewer::commands
