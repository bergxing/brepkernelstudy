#include "commands/tools/MoveTool.h"
#include "commands/tools/PreviewEdges.h"

#include "adapter/ISceneServiceFactory.h"
#include "commands/DocumentHistory.h"
#include "commands/snap/Accusnap.h"
#include "ecs/Systems.h"

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include <QCoreApplication>
#include <QMouseEvent>

namespace brep::viewer::commands
{
namespace
{

QString TrMove(const char* source)
{
    return QCoreApplication::translate("MoveTool", source);
}

}  // namespace

QString MoveTool::Prompt() const
{
    switch (m_step)
    {
        case Step::SelectObjects:
            return TrMove(
                "Move: click/drag/Ctrl+click to select, Space to confirm "
                "(ESC cancel)");
        case Step::PickBase:
            return TrMove("Move: pick base point (ESC cancel)");
        case Step::PickPlace:
            return TrMove("Move: pick destination (ESC cancel)");
    }
    return {};
}

void MoveTool::ClearPreview(CommandContext& ctx)
{
    if (ctx.ClearPreview)
    {
        ctx.ClearPreview();
    }
}

void MoveTool::OnStart(CommandContext& ctx)
{
    m_step = Step::SelectObjects;
    m_finished = false;
    m_result = CommandResult::Cancelled();
    m_targets.clear();
    if (ctx.SnapSessionRef)
    {
        ctx.SnapSessionRef->LastPoint.reset();
    }
    ClearPreview(ctx);

    const SelectedBodies selection = CollectSelectedBodies(ctx);
    if (AllMovable(selection))
    {
        m_targets = MovableBodyGuids(selection);
        m_step = Step::PickBase;
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(Prompt());
        }
        BREP_INFO("MoveTool start with {} object(s)", m_targets.size());
        return;
    }

    if (ctx.ReportStatus)
    {
        if (selection.SelectedCount > 0)
        {
            ctx.ReportStatus(
                TrMove("Move currently supports Box, Sphere, and Bezier"));
        }
        else
        {
            ctx.ReportStatus(Prompt());
        }
    }
    BREP_INFO("MoveTool start (select objects, then Space)");
}

bool MoveTool::ConfirmSelection(CommandContext& ctx)
{
    const SelectedBodies selection = CollectSelectedBodies(ctx);
    m_targets.clear();
    if (!AllMovable(selection))
    {
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(
                selection.SelectedCount > 0
                    ? TrMove(
                          "Move currently supports Box, Sphere, and Bezier")
                    : TrMove("Select objects to move, then press Space"));
        }
        return false;
    }
    m_targets = MovableBodyGuids(selection);
    m_step = Step::PickBase;
    ClearPreview(ctx);
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(Prompt());
    }
    BREP_INFO("MoveTool selection confirmed: {} object(s)", m_targets.size());
    return true;
}

bool MoveTool::PickGround(CommandContext& ctx, float x, float y,
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

void MoveTool::UpdatePreview(CommandContext& ctx, float x, float y)
{
    if (!ctx.SetPreviewEdges)
    {
        return;
    }
    Point3d place;
    if (!PickGround(ctx, x, y, place))
    {
        return;
    }

    EdgeMesh mesh = MakePointMarker(m_base);
    PushSegment(mesh, m_base, place);
    {
        EdgeMesh tip = MakePointMarker(place);
        mesh.Positions.insert(mesh.Positions.end(), tip.Positions.begin(),
                              tip.Positions.end());
    }

    const Vector3d offset{place.x() - m_base.x(), place.y() - m_base.y(),
                          place.z() - m_base.z()};
    if (ctx.Scene)
    {
        for (const Guid& bodyGuid : m_targets)
        {
            const auto bundle = ctx.Scene->MeshForBody(bodyGuid);
            EdgeMesh wire = TranslatedEdges(
                PreviewWire(bundle.Edges, bundle.Faces), offset);
            mesh.Positions.insert(mesh.Positions.end(), wire.Positions.begin(),
                                  wire.Positions.end());
        }
    }
    ctx.SetPreviewEdges(std::move(mesh));
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
}

void MoveTool::CommitMove(CommandContext& ctx, const Point3d& place)
{
    ClearPreview(ctx);
    if (!ctx.World || !ctx.World->Document() || !ctx.Scene)
    {
        m_result = CommandResult::Failed(TrMove("No active document"));
        m_finished = true;
        return;
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
        m_result = CommandResult::Failed(TrMove("No main part"));
        m_finished = true;
        return;
    }

    const Vector3d offset{place.x() - m_base.x(), place.y() - m_base.y(),
                          place.z() - m_base.z()};
    if (offset.norm() < 1e-6)
    {
        m_result = CommandResult::Failed(
            TrMove("Base and destination are the same point — pick farther"));
        m_finished = true;
        return;
    }

    RigidTransform transform;
    transform.Translation = Point3d{offset.x(), offset.y(), offset.z()};

    int moved = 0;
    for (const Guid& bodyGuid : m_targets)
    {
        if (!scene.TransformBody(bodyGuid, transform))
        {
            break;
        }
        ++moved;
    }
    if (moved != static_cast<int>(m_targets.size()))
    {
        if (moved > 0)
        {
            scene.UndoFeature(moved);
        }
        m_result = CommandResult::Failed(
            TrMove("Failed to move the selected object(s)"));
        m_finished = true;
        return;
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    ctx.World->SyncPartBodies(*part, std::move(material));

    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
    if (ctx.Session)
    {
        ctx.Session->MarkDirty();
    }

    const std::string wood = ctx.WoodAlbedoPath;
    ecs::World* world = ctx.World;
    Part* partPtr = part;
    const int undoSteps = moved;

    if (ctx.History)
    {
        ctx.History->push(DocumentHistory::Entry{
            .label = TrMove("Move %1 object(s)").arg(moved),
            .undo =
                [world, partPtr, wood, undoSteps, factory = ctx.SceneFactory,
                 session = ctx.Session, redraw = ctx.RequestRedraw,
                 refresh = ctx.RefreshUi]
            {
                if (!world || !partPtr)
                {
                    return;
                }
                adapter::WithScene(
                    factory, world->Document(),
                    [undoSteps](adapter::ISceneService& scene)
                    {
                        scene.UndoFeature(undoSteps);
                    });
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
                [world, partPtr, wood, undoSteps, factory = ctx.SceneFactory,
                 session = ctx.Session, redraw = ctx.RequestRedraw,
                 refresh = ctx.RefreshUi]
            {
                if (!world || !partPtr)
                {
                    return;
                }
                adapter::WithScene(
                    factory, world->Document(),
                    [undoSteps](adapter::ISceneService& scene)
                    {
                        scene.RedoFeature(undoSteps);
                    });
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

    m_result = CommandResult::Ok(TrMove("Moved %1 object(s)").arg(moved));
    m_finished = true;
    if (ctx.RefreshUi)
    {
        ctx.RefreshUi();
    }
    BREP_INFO("MoveTool committed {} move(s) offset=({:.4f},{:.4f},{:.4f})",
              moved, offset.x(), offset.y(), offset.z());
}

bool MoveTool::OnMousePress(CommandContext& ctx, float x, float y,
                              int button)
{
    if (m_step == Step::SelectObjects)
    {
        return false;
    }
    if (button != Qt::LeftButton)
    {
        return false;
    }

    Point3d hit;
    if (!PickGround(ctx, x, y, hit))
    {
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(
                TrMove("Missed surface/ground — try another angle"));
        }
        return true;
    }
    if (m_step == Step::PickBase)
    {
        m_base = hit;
        if (ctx.SnapSessionRef)
        {
            ctx.SnapSessionRef->LastPoint = hit;
        }
        m_step = Step::PickPlace;
        if (ctx.SetPreviewEdges)
        {
            ctx.SetPreviewEdges(MakePointMarker(m_base));
        }
        if (ctx.RequestRedraw)
        {
            ctx.RequestRedraw();
        }
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(Prompt());
        }
        return true;
    }

    const Vector3d offset{hit.x() - m_base.x(), hit.y() - m_base.y(),
                          hit.z() - m_base.z()};
    if (offset.norm() < 1e-6)
    {
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(TrMove(
                "Base and destination are the same point — pick farther"));
        }
        return true;
    }
    if (ctx.SnapSessionRef)
    {
        ctx.SnapSessionRef->LastPoint = hit;
    }
    CommitMove(ctx, hit);
    return true;
}

void MoveTool::OnMouseMove(CommandContext& ctx, float x, float y)
{
    if (m_step == Step::PickBase)
    {
        Point3d hover;
        (void)PickGround(ctx, x, y, hover);
    }
    else if (m_step == Step::PickPlace)
    {
        UpdatePreview(ctx, x, y);
    }
}

bool MoveTool::OnKeyPress(CommandContext& ctx, int key)
{
    if (m_step != Step::SelectObjects)
    {
        return false;
    }
    if (key != Qt::Key_Space && key != Qt::Key_Return && key != Qt::Key_Enter)
    {
        return false;
    }
    ConfirmSelection(ctx);
    return true;
}

void MoveTool::OnCancel(CommandContext& ctx)
{
    ClearPreview(ctx);
    m_finished = true;
    m_result = CommandResult::Cancelled(TrMove("Cancelled move"));
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(m_result.Message);
    }
}

}  // namespace brep::viewer::commands
