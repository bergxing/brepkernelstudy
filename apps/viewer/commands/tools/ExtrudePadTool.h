#pragma once

#include "commands/ITool.h"

#include "api/Core.h"

#include <vector>

namespace brep::viewer::commands
{

/// Closed profile on ground plane + extrude depth (Pad).
class ExtrudePadTool final : public ITool
{
public:
    [[nodiscard]] std::string_view Id() const noexcept override
    {
        return "part.extrude_pad";
    }
    [[nodiscard]] QString Prompt() const override;

    void OnStart(CommandContext& ctx) override;
    bool OnMousePress(CommandContext& ctx, float x, float y, int button) override;
    bool OnKeyPress(CommandContext& ctx, int key) override;
    void OnMouseMove(CommandContext& ctx, float x, float y) override;
    void OnCancel(CommandContext& ctx) override;

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
        Profile,
        Depth,
    };

    bool PickGround(CommandContext& ctx, float x, float y, Point3d& hit) const;
    bool PickHeight(CommandContext& ctx, float x, float y, double& height) const;
    void UpdatePreview(CommandContext& ctx, float x, float y);
    void ClearPreview(CommandContext& ctx);
    void CommitPad(CommandContext& ctx, double height);

    Step m_step{Step::Profile};
    std::vector<Point3d> m_profile3d;
    bool m_symmetric{false};
    bool m_finished{false};
    CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
