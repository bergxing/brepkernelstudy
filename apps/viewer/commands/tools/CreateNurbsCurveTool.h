#pragma once

#include "commands/ITool.h"

#include "api/Core.h"

#include <vector>

namespace brep::viewer::commands
{

/// Progressive cubic clamped NURBS: pick CVs, Enter to finish (min 4).
class CreateNurbsCurveTool final : public ITool
{
public:
    [[nodiscard]] std::string_view Id() const noexcept override
    {
        return "part.create_nurbs_curve";
    }
    [[nodiscard]] QString Prompt() const override;

    void OnStart(CommandContext& ctx) override;
    bool OnMousePress(CommandContext& ctx, float x, float y,
                      int button) override;
    void OnMouseMove(CommandContext& ctx, float x, float y) override;
    bool OnKeyPress(CommandContext& ctx, int key) override;
    bool OnContextMenu(CommandContext& ctx, float x, float y) override;
    void OnCancel(CommandContext& ctx) override;

    [[nodiscard]] bool OwnsUndoRedo() const noexcept override
    {
        return true;
    }
    [[nodiscard]] bool CanUndoStep() const noexcept override
    {
        return !m_cvs.empty();
    }
    [[nodiscard]] bool CanRedoStep() const noexcept override
    {
        return !m_undone.empty();
    }
    bool UndoStep(CommandContext& ctx) override;
    bool RedoStep(CommandContext& ctx) override;

    [[nodiscard]] bool AllowsViewportSelection() const noexcept override
    {
        return false;
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
        PickFirst,
        PickNext,
    };

    static constexpr int kMaxCvs = 256;

    [[nodiscard]] bool PickPoint(CommandContext& ctx, float x, float y,
                                 Point3d& hit) const;
    bool AcceptPoint(CommandContext& ctx, float x, float y);
    void FinishKeepOrAbort(CommandContext& ctx);
    void RefreshAfterEdit(CommandContext& ctx);
    void UpdatePreview(CommandContext& ctx, float x, float y);
    void ClearPreview(CommandContext& ctx);
    void CommitNurbs(CommandContext& ctx);

    Step m_step{Step::PickFirst};
    std::vector<Point3d> m_cvs;
    std::vector<Point3d> m_undone;
    bool m_hasHover{false};
    Point3d m_hover{};
    float m_lastX{0.0f};
    float m_lastY{0.0f};
    bool m_finished{false};
    CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
