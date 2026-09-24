#include "commands/tools/CopyTool.h"
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

#include <variant>

namespace brep::viewer::commands
{
namespace
{

QString TrCopy(const char* source)
{
    return QCoreApplication::translate("CopyTool", source);
}

void AppendCopyName(PrimitiveSpec& spec)
{
    std::visit(
        [](auto& s)
        {
            if (s.Name.empty())
            {
                s.Name = "copy";
            }
            s.Name += "_copy";
        },
        spec);
}

RigidTransform TranslationOf(const Vector3d& offset)
{
    RigidTransform transform;
    transform.Translation = Point3d{offset.x(), offset.y(), offset.z()};
    return transform;
}

}  // namespace

QString CopyTool::Prompt() const
{
    switch (m_step)
    {
        case Step::SelectObjects:
            return TrCopy(
                "Copy: click/drag/Ctrl+click to select, Space to confirm "
                "(ESC cancel)");
        case Step::PickBase:
            return TrCopy("Copy: pick base point (ESC cancel)");
        case Step::PickPlace:
            return TrCopy("Copy: pick destination (ESC cancel)");
    }
    return {};
}

void CopyTool::ClearPreview(CommandContext& ctx)
{
    if (ctx.ClearPreview)
    {
        ctx.ClearPreview();
    }
}

void CopyTool::OnStart(CommandContext& ctx)
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
    m_targets = CopyableBodies(selection);
    if (AllCopyable(selection))
    {
        m_step = Step::PickBase;
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(Prompt());
        }
        BREP_INFO("CopyTool start with {} object(s)", m_targets.size());
        return;
    }

    m_targets.clear();
    if (ctx.ReportStatus)
    {
        if (selection.SelectedCount > 0)
        {
            ctx.ReportStatus(TrCopy(
                "Copy currently supports Box, Sphere, Bezier, and NURBS curve"));
        }
        else
        {
            ctx.ReportStatus(Prompt());
        }
    }
    BREP_INFO("CopyTool start (select objects, then Space)");
}

bool CopyTool::ConfirmSelection(CommandContext& ctx)
{
    const SelectedBodies selection = CollectSelectedBodies(ctx);
    m_targets = CopyableBodies(selection);
    if (!AllCopyable(selection))
    {
        m_targets.clear();
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(
                selection.SelectedCount > 0
                    ? TrCopy(
                          "Copy currently supports Box, Sphere, Bezier, and NURBS curve")
                    : TrCopy("Select objects to copy, then press "
                             "Space"));
        }
        return false;
    }
    m_step = Step::PickBase;
    ClearPreview(ctx);
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(Prompt());
    }
    BREP_INFO("CopyTool selection confirmed: {} object(s)", m_targets.size());
    return true;
}

bool CopyTool::PickGround(CommandContext& ctx, float x, float y,
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

void CopyTool::UpdatePreview(CommandContext& ctx, float x, float y)
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
        for (const auto& target : m_targets)
        {
            if (!target.BodyGuid.IsValid())
            {
                continue;
            }
            const auto bundle = ctx.Scene->MeshForBody(target.BodyGuid);
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

void CopyTool::CommitCopies(CommandContext& ctx, const Point3d& place)
{
    ClearPreview(ctx);
    if (!ctx.World || !ctx.World->Document() || !ctx.Scene)
    {
        m_result = CommandResult::Failed(TrCopy("No active document"));
        m_finished = true;
        return;
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
        m_result = CommandResult::Failed(TrCopy("No main part"));
        m_finished = true;
        return;
    }

    const Vector3d offset{place.x() - m_base.x(), place.y() - m_base.y(),
                          place.z() - m_base.z()};
    if (offset.norm() < 1e-6)
    {
        m_result = CommandResult::Failed(
            TrCopy("Base and destination are the same point — pick farther"));
        m_finished = true;
        return;
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);

    const RigidTransform transform = TranslationOf(offset);
    int created = 0;
    for (const auto& target : m_targets)
    {
        Body* body = nullptr;
        std::optional<PrimitiveSpec> spec = target.Spec;
        if (!spec.has_value() && target.BodyGuid.IsValid())
        {
            spec = scene.SpecFor(Guid{}, target.BodyGuid);
        }
        if (spec.has_value())
        {
            auto moved = ApplyTransform(*spec, transform);
            if (!moved)
            {
                break;
            }
            AppendCopyName(*moved);
            body = scene.AddPrimitive(*moved);
            if (!body)
            {
                break;
            }
            auto obj = scene.ObjectForBody(body->Guid);
            if (!obj.has_value() || !obj->FeatureGuid.IsValid())
            {
                break;
            }
            scene.RecordAppendPrimitive(feat::FeatureId{obj->FeatureGuid},
                                        *moved);
        }
        else
        {
            body = scene.DuplicateBody(target.BodyGuid, transform);
            if (!body)
            {
                break;
            }
        }
        ++created;
    }

    if (created != static_cast<int>(m_targets.size()))
    {
        if (created > 0)
        {
            scene.UndoFeature(created);
        }
        m_result = CommandResult::Failed(TrCopy("Failed to create copies"));
        m_finished = true;
        return;
    }

    ctx.World->SyncPartBodies(*part, material);

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
    const int undoSteps = created;

    if (ctx.History)
    {
        ctx.History->push(DocumentHistory::Entry{
            .label = TrCopy("Copy %1 object(s)").arg(created),
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

    m_result = CommandResult::Ok(TrCopy("Copied %1 object(s)").arg(created));
    m_finished = true;
    if (ctx.RefreshUi)
    {
        ctx.RefreshUi();
    }
    BREP_INFO("CopyTool committed {} copies offset=({:.4f},{:.4f},{:.4f})",
              created, offset.x(), offset.y(), offset.z());
}

bool CopyTool::OnMousePress(CommandContext& ctx, float x, float y,
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
                TrCopy("Missed surface/ground — try another angle"));
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
            ctx.ReportStatus(TrCopy(
                "Base and destination are the same point — pick farther"));
        }
        return true;
    }
    if (ctx.SnapSessionRef)
    {
        ctx.SnapSessionRef->LastPoint = hit;
    }
    CommitCopies(ctx, hit);
    return true;
}

void CopyTool::OnMouseMove(CommandContext& ctx, float x, float y)
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

bool CopyTool::OnKeyPress(CommandContext& ctx, int key)
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

void CopyTool::OnCancel(CommandContext& ctx)
{
    ClearPreview(ctx);
    m_finished = true;
    m_result = CommandResult::Cancelled(TrCopy("Cancelled copy"));
    if (ctx.ReportStatus)
    {
        ctx.ReportStatus(m_result.Message);
    }
}

}  // namespace brep::viewer::commands
