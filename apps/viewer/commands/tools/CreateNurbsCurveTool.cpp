#include "commands/tools/CreateNurbsCurveTool.h"
#include "commands/tools/BezierPreview.h"
#include "commands/tools/PreviewEdges.h"

#include "adapter/ISceneServiceFactory.h"
#include "commands/DocumentHistory.h"
#include "commands/snap/Accusnap.h"

#include "api/Core.h"
#include "api/Mesh.h"
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

QString TrNurbs(const char* source)
{
    return QCoreApplication::translate("CreateNurbsCurveTool", source);
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

QString CreateNurbsCurveTool::Prompt() const
{
    if (m_step == Step::PickFirst)
    {
        return TrNurbs("Pick first point");
    }
    if (m_cvs.size() < 4)
    {
        return TrNurbs("Pick next point");
    }
    return TrNurbs("Pick next point, Enter to finish");
}

void CreateNurbsCurveTool::ClearPreview(CommandContext& ctx)
{
    if (ctx.ClearPreview)
    {
        ctx.ClearPreview();
    }
}

void CreateNurbsCurveTool::OnStart(CommandContext& ctx)
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

bool CreateNurbsCurveTool::PickPoint(CommandContext& ctx, float x, float y,
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

void CreateNurbsCurveTool::UpdatePreview(CommandContext& ctx, float x, float y)
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
        if (previewCvs.size() < 4)
        {
            AppendBezierCurvePreview(mesh, previewCvs);
        }
        else
        {
            NurbsCurve curve(previewCvs, {}, {});
            AppendPolylinePts(mesh, SampleNurbsPolyline(curve, 32));
        }
    }

    ctx.SetPreviewEdges(std::move(mesh));
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
}

void CreateNurbsCurveTool::CommitNurbs(CommandContext& ctx)
{
    ClearPreview(ctx);
    if (m_cvs.size() < 4)
    {
        m_result =
            CommandResult::Cancelled(TrNurbs("Cancelled create nurbs curve"));
        m_finished = true;
        return;
    }
    if (!ctx.World || !ctx.World->Document() || !ctx.Scene)
    {
        m_result = CommandResult::Failed(TrNurbs("No active document"));
        m_finished = true;
        return;
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
        m_result = CommandResult::Failed(TrNurbs("No main part"));
        m_finished = true;
        return;
    }

    NurbsCurveSpec spec;
    spec.Cvs = m_cvs;
    spec.Degree = 3;
    spec.Weights.clear();
    spec.Knots.clear();
    spec.Name = "nurbs";

    Body* body = scene.AddPrimitive(spec);
    if (!body)
    {
        m_result =
            CommandResult::Failed(TrNurbs("Failed to create nurbs curve"));
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
    const std::size_t cvCount = m_cvs.size();
    BREP_INFO("CreateNurbsCurveTool committed guid={} cvs={}",
              guid.ToString(),
              cvCount);

    ecs::World* world = ctx.World;
    Part* partPtr = part;

    if (ctx.History)
    {
        ctx.History->push(DocumentHistory::Entry{
            .label = TrNurbs("Create nurbs curve"),
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
        TrNurbs("Created nurbs curve %1")
            .arg(QString::fromStdString(guid.ToString())));
    m_finished = true;
}

void CreateNurbsCurveTool::FinishKeepOrAbort(CommandContext& ctx)
{
    if (m_cvs.size() >= 4)
    {
        CommitNurbs(ctx);
        return;
    }
    ClearPreview(ctx);
    m_finished = true;
    m_result =
        CommandResult::Cancelled(TrNurbs("Cancelled create nurbs curve"));
}

bool CreateNurbsCurveTool::AcceptPoint(CommandContext& ctx, float x, float y)
{
    if (static_cast<int>(m_cvs.size()) >= kMaxCvs)
    {
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(TrNurbs("At most 256 control points"));
        }
        return true;
    }
    Point3d hit;
    if (!PickPoint(ctx, x, y, hit))
    {
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(
                TrNurbs("Missed surface/ground — try another angle"));
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
                    TrNurbs("Points too close — pick farther"));
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
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(Prompt());
    }
    UpdatePreview(ctx, x, y);
    return true;
}

bool CreateNurbsCurveTool::OnMousePress(CommandContext& ctx, float x, float y,
                                        int button)
{
    if (button != Qt::LeftButton)
    {
        return false;
    }
    return AcceptPoint(ctx, x, y);
}

void CreateNurbsCurveTool::OnMouseMove(CommandContext& ctx, float x, float y)
{
    UpdatePreview(ctx, x, y);
}

bool CreateNurbsCurveTool::OnKeyPress(CommandContext& ctx, int key)
{
    if (key == Qt::Key_Return || key == Qt::Key_Enter)
    {
        if (m_hasHover)
        {
            return AcceptPoint(ctx, m_lastX, m_lastY);
        }
        if (m_cvs.size() >= 4)
        {
            CommitNurbs(ctx);
            return true;
        }
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(TrNurbs("Need at least 4 control points"));
        }
        return true;
    }
    if (key != Qt::Key_Backspace)
    {
        return false;
    }
    return UndoStep(ctx);
}

void CreateNurbsCurveTool::RefreshAfterEdit(CommandContext& ctx)
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

bool CreateNurbsCurveTool::UndoStep(CommandContext& ctx)
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

bool CreateNurbsCurveTool::RedoStep(CommandContext& ctx)
{
    if (m_undone.empty())
    {
        return false;
    }
    if (static_cast<int>(m_cvs.size()) >= kMaxCvs)
    {
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(TrNurbs("At most 256 control points"));
        }
        return true;
    }
    m_cvs.push_back(m_undone.back());
    m_undone.pop_back();
    RefreshAfterEdit(ctx);
    return true;
}

bool CreateNurbsCurveTool::OnContextMenu(CommandContext& ctx, float x, float y)
{
    QMenu menu(ctx.ParentWidget);
    menu.setCursor(Qt::ArrowCursor);
    QAction* actConfirm = menu.addAction(TrNurbs("Confirm"));
    QAction* actCancel = menu.addAction(TrNurbs("Cancel"));

    MenuEscFilter filter(&menu);
    menu.installEventFilter(&filter);
    const QPoint menuPos = QCursor::pos();
    BREP_INFO(
        "nurbs context menu show cvs={} pick=({:.1f},{:.1f}) global=({},{})",
        m_cvs.size(),
        x,
        y,
        menuPos.x(),
        menuPos.y());
    ScopedMenuArrowCursor arrowCursor;
    QAction* chosen = menu.exec(menuPos);
    if (m_finished)
    {
        BREP_INFO("nurbs context menu ignored: tool already finished");
        return true;
    }
    if (filter.undoPressed)
    {
        BREP_INFO("nurbs context menu Ctrl+Z cvs={}", m_cvs.size());
        UndoStep(ctx);
        return true;
    }
    if (filter.redoPressed)
    {
        BREP_INFO("nurbs context menu Ctrl+Y cvs={}", m_cvs.size());
        RedoStep(ctx);
        return true;
    }
    if (chosen == actConfirm)
    {
        BREP_INFO("nurbs context menu chosen Confirm cvs={}", m_cvs.size());
        return AcceptPoint(ctx, x, y);
    }
    if (chosen == actCancel || filter.escPressed)
    {
        BREP_INFO(
            "nurbs context menu {} cvs={}",
            filter.escPressed ? "ESC" : "Cancel",
            m_cvs.size());
        FinishKeepOrAbort(ctx);
        return true;
    }
    BREP_INFO("nurbs context menu dismissed cvs={}", m_cvs.size());
    return true;
}

void CreateNurbsCurveTool::OnCancel(CommandContext& ctx)
{
    FinishKeepOrAbort(ctx);
}

}  // namespace brep::viewer::commands
