#pragma once

#include "commands/ITool.h"
#include "commands/tools/SelectedBodies.h"

#include "api/Core.h"
#include "api/Modeling.h"

#include <vector>

namespace brep::viewer::commands
{

/// Copy selected bodies: primitives via SpecFor, result bodies via DuplicateBody.
class CopyTool final : public ITool
{
public:
    [[nodiscard]] std::string_view Id() const noexcept override
    {
        return "edit.copy";
    }
    [[nodiscard]] QString Prompt() const override;

    void OnStart(CommandContext& ctx) override;
    bool OnMousePress(CommandContext& ctx, float x, float y, int button) override;
    void OnMouseMove(CommandContext& ctx, float x, float y) override;
    bool OnKeyPress(CommandContext& ctx, int key) override;
    void OnCancel(CommandContext& ctx) override;

    [[nodiscard]] bool AllowsViewportSelection() const noexcept override
    {
        return m_step == Step::SelectObjects;
    }

    [[nodiscard]] bool IsFinished() const noexcept override
    {
        return m_finished;
    }
    [[nodiscard]] CommandResult Result() const override
    {
        return m_result;
    }

private:
    enum class Step
    {
        SelectObjects,
        PickBase,
        PickPlace,
    };

    bool PickGround(CommandContext& ctx, float x, float y, Point3d& hit) const;
    bool ConfirmSelection(CommandContext& ctx);
    void UpdatePreview(CommandContext& ctx, float x, float y);
    void ClearPreview(CommandContext& ctx);
    void CommitCopies(CommandContext& ctx, const Point3d& place);

    Step m_step{Step::SelectObjects};
    Point3d m_base{};
    std::vector<SelectedBody> m_targets;
    bool m_finished{false};
    CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
