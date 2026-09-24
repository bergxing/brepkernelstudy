#pragma once

#include "commands/ITool.h"

#include "api/Core.h"

namespace brep::viewer::commands
{

/// Two-point sphere: center (mesh or ground), then radius point.
class CreateSphereTool final : public ITool
{
public:
    [[nodiscard]] std::string_view Id() const noexcept override
    {
        return "part.create_sphere";
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
        Center,
        Radius,
    };

    /// Mesh surface first; else y=0 ground.
    bool PickPoint(CommandContext& ctx, float x, float y, Point3d& hit) const;
    void UpdatePreview(CommandContext& ctx, float x, float y);
    void ClearPreview(CommandContext& ctx);
    void CommitSphere(CommandContext& ctx, double radius);

    Step m_step{Step::Center};
    Point3d m_center{};
    bool m_finished{false};
    CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
