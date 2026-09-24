#pragma once

#include "commands/ITool.h"

#include "api/Core.h"

namespace brep::viewer::commands
{

/// Three-point box: base corner, opposite corner, then height.
class CreateBoxTool final : public ITool
{
public:
    [[nodiscard]] std::string_view Id() const noexcept override
    {
        return "part.create_box";
    }
    [[nodiscard]] QString Prompt() const override;

    void OnStart(CommandContext& ctx) override;
    bool OnMousePress(CommandContext& ctx, float x, float y, int button) override;
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
        FirstCorner,
        OppositeCorner,
        Height,
    };

    bool PickGround(CommandContext& ctx, float x, float y, Point3d& hit) const;
    bool PickHeight(CommandContext& ctx, float x, float y, double& height) const;
    void UpdatePreview(CommandContext& ctx, float x, float y);
    void ClearPreview(CommandContext& ctx);
    void CommitBox(CommandContext& ctx, double height);

    Step m_step{Step::FirstCorner};
    Point3d m_cornerA{};
    Point3d m_cornerB{};
    bool m_finished{false};
    CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
