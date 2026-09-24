#pragma once

#include "commands/ITool.h"

#include "api/Modeling.h"

#include <entt/entt.hpp>

namespace brep::viewer::commands
{

/// Union / Subtract / Intersect: first pick is the target, second is the tool.
class BooleanTwoBodyTool final : public ITool
{
public:
    explicit BooleanTwoBodyTool(boolean::BooleanOp op);

    [[nodiscard]] std::string_view Id() const noexcept override;
    [[nodiscard]] QString Prompt() const override;

    void OnStart(CommandContext& ctx) override;
    bool OnMousePress(CommandContext& ctx, float x, float y,
                      int button) override;
    void OnMouseMove(CommandContext& ctx, float x, float y) override;
    bool OnKeyPress(CommandContext& ctx, int key) override;
    bool OnContextMenu(CommandContext& ctx, float x, float y) override;
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
        SelectTarget,
        SelectTool,
    };

    [[nodiscard]] QString OpNoun() const;
    [[nodiscard]] QString HistoryLabel() const;
    [[nodiscard]] QString FailMessage(const QString& targetName,
                                      const QString& toolName) const;

    void ApplyTarget(CommandContext& ctx, entt::entity hit);
    void ApplyTool(CommandContext& ctx, entt::entity hit);
    bool TryCommit(CommandContext& ctx);
    [[nodiscard]] bool HasBothOperands() const noexcept;

    boolean::BooleanOp m_op{boolean::BooleanOp::Subtract};
    Step m_step{Step::SelectTarget};
    entt::entity m_target{entt::null};
    entt::entity m_tool{entt::null};
    bool m_finished{false};
    CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
