#include "commands/tools/CreateBezierTool.h"
#include "commands/tools/BezierPreview.h"
#include "commands/tools/PreviewEdges.h"

#include "adapter/ISceneServiceFactory.h"
#include "commands/DocumentHistory.h"
#include "commands/snap/Accusnap.h"

#include "api/Core.h"
#include "api/Modeling.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QObject>

namespace brep::viewer::commands
{
namespace
{

QString TrBezier(const char* source)
{
    return QCoreApplication::translate("CreateBezierTool", source);
}

[[nodiscard]] const char* SpecNameForDegree(int degree) noexcept
{
    if (degree <= 1)
    {
        return "line";
    }
    if (degree == 2)
    {
        return "parabola";
    }
    return "bezier";
}

class MenuEscFilter final : public QObject
{
public:
    explicit MenuEscFilter(QMenu* menu) : m_menu(menu) {}

    bool escPressed{false};
    bool undoPressed{false};
    bool redoPressed{false};

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::KeyPress)
        {
            return QObject::eventFilter(watched, event);
        }
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape)
        {
            escPressed = true;
            return QObject::eventFilter(watched, event);
        }
        if (ke->matches(QKeySequence::Undo))
        {
            undoPressed = true;
            if (m_menu)
            {
                m_menu->close();
            }
            return true;
        }
        if (ke->matches(QKeySequence::Redo))
        {
            redoPressed = true;
            if (m_menu)
            {
                m_menu->close();
            }
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QMenu* m_menu{nullptr};
};

class ScopedMenuArrowCursor
{
public:
    ScopedMenuArrowCursor()
    {
        if (QApplication::overrideCursor() != nullptr)
        {
            QApplication::changeOverrideCursor(Qt::ArrowCursor);
            m_changed = true;
        }
    }

    ~ScopedMenuArrowCursor()
    {
        if (m_changed && QApplication::overrideCursor() != nullptr)
        {
            QApplication::changeOverrideCursor(Qt::CrossCursor);
        }
    }

    ScopedMenuArrowCursor(const ScopedMenuArrowCursor&) = delete;
    ScopedMenuArrowCursor& operator=(const ScopedMenuArrowCursor&) = delete;

private:
    bool m_changed{false};
};

}  // namespace

QString CreateBezierTool::Prompt() const
{
    if (m_step == Step::PickFirst)
    {
        return TrBezier("Pick first point");
    }
    return TrBezier("Pick next point");
}

void CreateBezierTool::ClearPreview(CommandContext& ctx)
{
    if (ctx.ClearPreview)
    {
        ctx.ClearPreview();
    }
}

void CreateBezierTool::OnStart(CommandContext& ctx)
{
    m_step = Step::PickFirst;
    m_cvs.clear();
    m_undone.clear();
    m_hasHover = false;
    m_finished = false;
    m_result = CommandResult::Cancelled();
    if (ctx.SnapSessionRef)
    {
        ctx.SnapSessionRef->LastPoint.reset();
    }
    ClearPreview(ctx);
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(Prompt());
    }
}

bool CreateBezierTool::PickPoint(CommandContext& ctx, float x, float y,
                                 Point3d& hit) const
{
    const PickResult result = AccuSnap::Resolve(ctx, x, y);
    if (result.Kind == SnapKind::None)
    {
        return false;
    }
    hit = result.Point;
    return true;
}

void CreateBezierTool::UpdatePreview(CommandContext& ctx, float x, float y)
{
    m_lastX = x;
    m_lastY = y;
    Point3d hover;
    if (!PickPoint(ctx, x, y, hover))
    {
        return;
    }
    m_hover = hover;
    m_hasHover = true;
    if (!ctx.SetPreviewEdges)
    {
        return;
    }

    EdgeMesh mesh;
    for (const Point3d& p : m_cvs)
    {
        EdgeMesh tip = MakePointMarker(p);
        mesh.Positions.insert(mesh.Positions.end(), tip.Positions.begin(),
                              tip.Positions.end());
    }
    {
        EdgeMesh tip = MakePointMarker(hover);
        mesh.Positions.insert(mesh.Positions.end(), tip.Positions.begin(),
                              tip.Positions.end());
    }

    if (!m_cvs.empty())
    {
        std::vector<Point3d> previewCvs = m_cvs;
        previewCvs.push_back(hover);
        for (std::size_t i = 1; i < previewCvs.size(); ++i)
        {
            PushSegment(mesh, previewCvs[i - 1], previewCvs[i]);
        }
        AppendBezierCurvePreview(mesh, previewCvs);
    }

    ctx.SetPreviewEdges(std::move(mesh));
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
}

void CreateBezierTool::CommitBezier(CommandContext& ctx)
{
    ClearPreview(ctx);
    if (m_cvs.size() < 2)
    {
        m_result = CommandResult::Cancelled();
        m_finished = true;
        return;
    }
    if (!ctx.World || !ctx.World->Document() || !ctx.Scene)
    {
        m_result = CommandResult::Failed(TrBezier("No active document"));
        m_finished = true;
        return;
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
        m_result = CommandResult::Failed(TrBezier("No main part"));
        m_finished = true;
        return;
    }

    BezierSpec spec;
    spec.Cvs = m_cvs;
    spec.Degree = static_cast<int>(m_cvs.size()) - 1;
    spec.SegmentCount = 1;
    spec.Weights.assign(m_cvs.size(), 1.0);
    spec.Name = SpecNameForDegree(spec.Degree);

    Body* body = scene.AddPrimitive(spec);
    if (!body)
    {
        m_result = CommandResult::Failed(TrBezier("Failed to create bezier"));
        m_finished = true;
        return;
    }

    const feat::FeatureId fid =
        scene.FeatureIdFor(Guid{}, body->Guid).value_or(feat::FeatureId{});
    if (fid.IsValid())
    {
        scene.RecordAppendPrimitive(fid, spec);
    }
    const std::string wood = ctx.WoodAlbedoPath;
    Material mat = wood.empty() ? Material{} : MakeWoodMaterial(wood);
    ctx.World->SyncPartBodies(*part, std::move(mat));
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
    if (ctx.Session)
    {
        ctx.Session->MarkDirty();
    }

    const Guid guid = body->Guid;
    ecs::World* world = ctx.World;
    Part* partPtr = part;

    if (ctx.History)
    {
        ctx.History->push(DocumentHistory::Entry{
            .label = TrBezier("Create bezier"),
            .undo =
                [world, partPtr, wood, factory = ctx.SceneFactory,
                 session = ctx.Session, redraw = ctx.RequestRedraw,
                 refresh = ctx.RefreshUi]
                {
                    if (!world || !partPtr)
                    {
                        return;
                    }
                    adapter::WithScene(
                        factory, world->Document(),
                        [](adapter::ISceneService& s) { s.UndoFeature(); });
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
                    if (!world || !partPtr)
                    {
                        return;
                    }
                    adapter::WithScene(
                        factory, world->Document(),
                        [](adapter::ISceneService& s) { s.RedoFeature(); });
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

    m_result = CommandResult::Ok(
        TrBezier("Created bezier %1")
            .arg(QString::fromStdString(guid.ToString())));
    m_finished = true;
}

void CreateBezierTool::FinishKeepOrAbort(CommandContext& ctx)
{
    if (m_cvs.size() >= 2)
    {
        CommitBezier(ctx);
        return;
    }
    ClearPreview(ctx);
    m_finished = true;
    m_result = CommandResult::Cancelled();
}

bool CreateBezierTool::AcceptPoint(CommandContext& ctx, float x, float y)
{
    if (static_cast<int>(m_cvs.size()) >= kMaxCvs)
    {
        CommitBezier(ctx);
        return true;
    }
    Point3d hit;
    if (!PickPoint(ctx, x, y, hit))
    {
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(
                TrBezier("Missed surface/ground — try another angle"));
        }
        return true;
    }
    if (!m_cvs.empty())
    {
        const Vector3d d = hit - m_cvs.back();
        if (d.norm() < 1e-6)
        {
            if (ctx.ReportStatus)
            {
                ctx.ReportStatus(
                    TrBezier("Points too close — pick farther"));
            }
            return true;
        }
    }

    if (ctx.SnapSessionRef)
    {
        ctx.SnapSessionRef->LastPoint = hit;
    }

    m_cvs.push_back(hit);
    m_undone.clear();
    m_step = Step::PickNext;
    if (static_cast<int>(m_cvs.size()) >= kMaxCvs)
    {
        CommitBezier(ctx);
        return true;
    }
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(Prompt());
    }
    UpdatePreview(ctx, x, y);
    return true;
}

bool CreateBezierTool::OnMousePress(CommandContext& ctx, float x, float y,
                                    int button)
{
    if (button != Qt::LeftButton)
    {
        return false;
    }
    return AcceptPoint(ctx, x, y);
}

void CreateBezierTool::OnMouseMove(CommandContext& ctx, float x, float y)
{
    UpdatePreview(ctx, x, y);
}

bool CreateBezierTool::OnKeyPress(CommandContext& ctx, int key)
{
    if (key == Qt::Key_Return || key == Qt::Key_Enter)
    {
        if (m_hasHover)
        {
            return AcceptPoint(ctx, m_lastX, m_lastY);
        }
        if (m_cvs.size() >= 2)
        {
            CommitBezier(ctx);
            return true;
        }
        return true;
    }
    if (key != Qt::Key_Backspace)
    {
        return false;
    }
    return UndoStep(ctx);
}

void CreateBezierTool::RefreshAfterEdit(CommandContext& ctx)
{
    m_step = m_cvs.empty() ? Step::PickFirst : Step::PickNext;
    if (m_hasHover)
    {
        UpdatePreview(ctx, m_lastX, m_lastY);
    }
    else
    {
        ClearPreview(ctx);
        if (ctx.RequestRedraw)
        {
            ctx.RequestRedraw();
        }
    }
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(Prompt());
    }
    if (ctx.RefreshUi)
    {
        ctx.RefreshUi();
    }
}

bool CreateBezierTool::UndoStep(CommandContext& ctx)
{
    if (m_cvs.empty())
    {
        return false;
    }
    m_undone.push_back(m_cvs.back());
    m_cvs.pop_back();
    RefreshAfterEdit(ctx);
    return true;
}

bool CreateBezierTool::RedoStep(CommandContext& ctx)
{
    if (m_undone.empty())
    {
        return false;
    }
    m_cvs.push_back(m_undone.back());
    m_undone.pop_back();
    if (static_cast<int>(m_cvs.size()) >= kMaxCvs)
    {
        CommitBezier(ctx);
        return true;
    }
    RefreshAfterEdit(ctx);
    return true;
}

bool CreateBezierTool::OnContextMenu(CommandContext& ctx, float x, float y)
{
    QMenu menu(ctx.ParentWidget);
    menu.setCursor(Qt::ArrowCursor);
    QAction* actConfirm = menu.addAction(TrBezier("Confirm"));
    QAction* actCancel = menu.addAction(TrBezier("Cancel"));

    MenuEscFilter filter(&menu);
    menu.installEventFilter(&filter);
    const QPoint menuPos = QCursor::pos();
    BREP_INFO(
        "bezier context menu show cvs={} pick=({:.1f},{:.1f}) global=({},{})",
        m_cvs.size(),
        x,
        y,
        menuPos.x(),
        menuPos.y());
    ScopedMenuArrowCursor arrowCursor;
    QAction* chosen = menu.exec(menuPos);
    if (m_finished)
    {
        BREP_INFO("bezier context menu ignored: tool already finished");
        return true;
    }
    if (filter.undoPressed)
    {
        BREP_INFO("bezier context menu Ctrl+Z cvs={}", m_cvs.size());
        UndoStep(ctx);
        return true;
    }
    if (filter.redoPressed)
    {
        BREP_INFO("bezier context menu Ctrl+Y cvs={}", m_cvs.size());
        RedoStep(ctx);
        return true;
    }
    if (chosen == actConfirm)
    {
        BREP_INFO("bezier context menu chosen Confirm cvs={}", m_cvs.size());
        return AcceptPoint(ctx, x, y);
    }
    if (chosen == actCancel || filter.escPressed)
    {
        BREP_INFO(
            "bezier context menu {} cvs={}",
            filter.escPressed ? "ESC" : "Cancel",
            m_cvs.size());
        FinishKeepOrAbort(ctx);
        return true;
    }
    BREP_INFO("bezier context menu dismissed cvs={}", m_cvs.size());
    return true;
}

void CreateBezierTool::OnCancel(CommandContext& ctx)
{
    FinishKeepOrAbort(ctx);
}

}  // namespace brep::viewer::commands
