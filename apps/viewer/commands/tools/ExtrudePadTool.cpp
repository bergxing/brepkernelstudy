#include "commands/tools/ExtrudePadTool.h"
#include "commands/tools/PreviewEdges.h"

#include "adapter/ISceneServiceFactory.h"
#include "Camera.h"
#include "commands/DocumentHistory.h"
#include "commands/Picking.h"
#include "commands/snap/Accusnap.h"

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include "ecs/World.h"

#include <QCoreApplication>
#include <QKeyEvent>

#include <algorithm>
#include <cmath>

namespace brep::viewer::commands
{
namespace
{

QString TrPad(const char* source)
{
    return QCoreApplication::translate("ExtrudePadTool", source);
}

[[nodiscard]] Point2d ToProfileUv(const Point3d& p)
{
    return Point2d{p.x(), p.z()};
}

[[nodiscard]] Point3d ProfileToWorld(const Point2d& uv, double y = 0.0)
{
    return Point3d{uv.u(), y, uv.v()};
}

void AppendCap(EdgeMesh& mesh, const std::vector<Point2d>& profile, double y)
{
    if (profile.size() < 2)
    {
        return;
    }
    for (std::size_t i = 0; i < profile.size(); ++i)
    {
        const std::size_t j = (i + 1) % profile.size();
        PushSegment(mesh, ProfileToWorld(profile[i], y),
                    ProfileToWorld(profile[j], y));
    }
}

void AppendSide(EdgeMesh& mesh, const Point2d& a, const Point2d& b, double y0,
                double y1)
{
    PushSegment(mesh, ProfileToWorld(a, y0), ProfileToWorld(a, y1));
    PushSegment(mesh, ProfileToWorld(b, y0), ProfileToWorld(b, y1));
    PushSegment(mesh, ProfileToWorld(a, y0), ProfileToWorld(b, y0));
    PushSegment(mesh, ProfileToWorld(a, y1), ProfileToWorld(b, y1));
}

[[nodiscard]] EdgeMesh MakeExtrudeWire(const std::vector<Point2d>& profile,
                                       double distance, bool symmetric)
{
    EdgeMesh mesh;
    if (profile.size() < 3)
    {
        return mesh;
    }
    const double d0 = symmetric ? -0.5 * distance : 0.0;
    const double d1 = symmetric ? 0.5 * distance : distance;
    AppendCap(mesh, profile, d0);
    AppendCap(mesh, profile, d1);
    for (std::size_t i = 0; i < profile.size(); ++i)
    {
        const std::size_t j = (i + 1) % profile.size();
        AppendSide(mesh, profile[i], profile[j], d0, d1);
    }
    return mesh;
}

[[nodiscard]] bool NearPoint(const Point3d& a, const Point3d& b, double eps)
{
    return std::abs(a.x() - b.x()) < eps && std::abs(a.y() - b.y()) < eps &&
           std::abs(a.z() - b.z()) < eps;
}

}  // namespace

QString ExtrudePadTool::Prompt() const
{
    switch (m_step)
    {
        case Step::Profile:
            return TrPad("Extrude: pick profile points (click first point to "
                         "close, ESC cancel)");
        case Step::Depth:
            if (m_symmetric)
            {
                return TrPad("Extrude: pick total depth — Symmetric ON (S "
                             "toggle, ESC cancel)");
            }
            return TrPad("Extrude: pick depth (S = symmetric, ESC cancel)");
    }
    return {};
}

void ExtrudePadTool::ClearPreview(CommandContext& ctx)
{
    if (ctx.ClearPreview)
    {
        ctx.ClearPreview();
    }
}

void ExtrudePadTool::OnStart(CommandContext& ctx)
{
    m_step = Step::Profile;
    m_profile3d.clear();
    m_symmetric = false;
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

bool ExtrudePadTool::PickGround(CommandContext& ctx, float x, float y,
                                 Point3d& hit) const
{
    Camera* cam = ctx.ViewCamera ? ctx.ViewCamera
                                 : (ctx.World ? ctx.World->MainCamera() : nullptr);
    Point3d origin;
    Vector3d dir;
    Point3d workplane;
    const bool haveWorkplane =
        cam != nullptr &&
        ScreenToRay(*cam, ctx.ViewportWidth, ctx.ViewportHeight, x, y, origin,
                    dir) &&
        IntersectPlaneY(origin, dir, 0.0, workplane);

    const PickResult result = AccuSnap::Resolve(ctx, x, y);
    if (result.Kind != SnapKind::None && std::abs(result.Point.y()) <= 1e-3)
    {
        hit = Point3d{result.Point.x(), 0.0, result.Point.z()};
        return true;
    }
    if (haveWorkplane)
    {
        hit = workplane;
        return true;
    }
    return false;
}

bool ExtrudePadTool::PickHeight(CommandContext& ctx, float x, float y,
                                 double& height) const
{
    Camera* cam = ctx.ViewCamera ? ctx.ViewCamera
                                 : (ctx.World ? ctx.World->MainCamera() : nullptr);
    if (!cam || m_profile3d.empty())
    {
        return false;
    }

    Point3d origin;
    Vector3d dir;
    if (!ScreenToRay(*cam, ctx.ViewportWidth, ctx.ViewportHeight, x, y, origin,
                     dir))
    {
        return false;
    }

    double cx = 0.0;
    double cz = 0.0;
    for (const Point3d& p : m_profile3d)
    {
        cx += p.x();
        cz += p.z();
    }
    cx /= static_cast<double>(m_profile3d.size());
    cz /= static_cast<double>(m_profile3d.size());
    const Point3d pivot{cx, 0.0, cz};

    const Point3d eye = cam->eye();
    Vector3d n{eye.x() - pivot.x(), 0.0, eye.z() - pivot.z()};
    if (n.norm() < 1e-6)
    {
        n = Vector3d{1.0, 0.0, 0.0};
    }
    n = n.normalized();

    Point3d hit;
    if (!IntersectPlane(origin, dir, pivot, n, hit))
    {
        return false;
    }
    height = hit.y();
    return true;
}

void ExtrudePadTool::UpdatePreview(CommandContext& ctx, float x, float y)
{
    if (!ctx.SetPreviewEdges && !ctx.SetPreview)
    {
        return;
    }

    std::vector<Point2d> profile;
    profile.reserve(m_profile3d.size() + 1);
    for (const Point3d& p : m_profile3d)
    {
        profile.push_back(ToProfileUv(p));
    }

    if (m_step == Step::Profile)
    {
        EdgeMesh mesh;
        for (const Point3d& p : m_profile3d)
        {
            const EdgeMesh marker = MakePointMarker(p, 0.08);
            for (std::size_t i = 0; i + 1 < marker.Positions.size(); i += 2)
            {
                PushSegment(mesh, marker.Positions[i], marker.Positions[i + 1]);
            }
        }
        if (profile.size() >= 1)
        {
            Point3d hover;
            if (PickGround(ctx, x, y, hover))
            {
                profile.push_back(ToProfileUv(hover));
            }
        }
        if (profile.size() >= 2)
        {
            for (std::size_t i = 0; i + 1 < profile.size(); ++i)
            {
                PushSegment(mesh, ProfileToWorld(profile[i]),
                            ProfileToWorld(profile[i + 1]));
            }
        }
        if (ctx.SetPreviewEdges)
        {
            ctx.SetPreviewEdges(std::move(mesh));
        }
        return;
    }

    double height = 0.0;
    if (!PickHeight(ctx, x, y, height))
    {
        return;
    }
    if (std::abs(height) < 1e-4)
    {
        height = (height < 0.0) ? -1e-3 : 1e-3;
    }
    const EdgeMesh wire =
        MakeExtrudeWire(profile, std::abs(height), m_symmetric);
    if (ctx.SetPreviewEdges)
    {
        ctx.SetPreviewEdges(wire);
    }
}

bool ExtrudePadTool::OnMousePress(CommandContext& ctx, float x, float y,
                                   int button)
{
    if (button != Qt::LeftButton || m_finished)
    {
        return false;
    }

    if (m_step == Step::Profile)
    {
        Point3d hit;
        if (!PickGround(ctx, x, y, hit))
        {
            return true;
        }
        if (!m_profile3d.empty() &&
            NearPoint(hit, m_profile3d.front(), 0.15) &&
            m_profile3d.size() >= 3)
        {
            m_step = Step::Depth;
            if (ctx.ReportStatus)
            {
                ctx.ReportStatus(Prompt());
            }
            UpdatePreview(ctx, x, y);
            return true;
        }
        if (!m_profile3d.empty() && NearPoint(hit, m_profile3d.back(), 1e-4))
        {
            if (ctx.ReportStatus)
            {
                ctx.ReportStatus(TrPad("Points too close — pick farther"));
            }
            return true;
        }
        m_profile3d.push_back(hit);
        if (ctx.SnapSessionRef)
        {
            ctx.SnapSessionRef->LastPoint = hit;
        }
        UpdatePreview(ctx, x, y);
        if (ctx.RequestRedraw)
        {
            ctx.RequestRedraw();
        }
        return true;
    }

    double height = 0.0;
    if (!PickHeight(ctx, x, y, height))
    {
        return true;
    }
    CommitPad(ctx, height);
    return true;
}

bool ExtrudePadTool::OnKeyPress(CommandContext& ctx, int key)
{
    if (m_finished || m_step != Step::Depth)
    {
        return false;
    }
    if (key == Qt::Key_S)
    {
        m_symmetric = !m_symmetric;
        if (ctx.ReportStatus)
        {
            ctx.ReportStatus(Prompt());
        }
        return true;
    }
    return false;
}

void ExtrudePadTool::OnMouseMove(CommandContext& ctx, float x, float y)
{
    UpdatePreview(ctx, x, y);
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
}

void ExtrudePadTool::OnCancel(CommandContext& ctx)
{
    ClearPreview(ctx);
    m_result = CommandResult::Cancelled(TrPad("Extrude cancelled"));
    m_finished = true;
}

void ExtrudePadTool::CommitPad(CommandContext& ctx, double height)
{
    using namespace brep;
    ClearPreview(ctx);

    if (!ctx.Scene)
    {
        m_result = CommandResult::Failed(TrPad("No active document"));
        m_finished = true;
        return;
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
        m_result = CommandResult::Failed(TrPad("No main part"));
        m_finished = true;
        return;
    }

    if (m_profile3d.size() < 3 || std::abs(height) < 1e-4)
    {
        m_result = CommandResult::Failed(
            TrPad("Profile or depth too small — pick again"));
        m_finished = true;
        return;
    }

    std::vector<Point2d> profile;
    profile.reserve(m_profile3d.size());
    for (const Point3d& p : m_profile3d)
    {
        profile.push_back(ToProfileUv(p));
    }

    const double distance = std::abs(height);
    Body* body = scene.AddExtrudePad(profile, distance, m_symmetric, "Pad");
    if (!body)
    {
        m_result = CommandResult::Failed(TrPad("Extrude failed"));
        m_finished = true;
        return;
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    ctx.World->SyncPartBodies(*part, material);
    if (ctx.RequestRedraw)
    {
        ctx.RequestRedraw();
    }
    if (ctx.Session)
    {
        ctx.Session->MarkDirty();
    }

    ecs::World* world = ctx.World;
    Part* part_ptr = part;
    const std::string wood = ctx.WoodAlbedoPath;
    if (ctx.History)
    {
        ctx.History->push(DocumentHistory::Entry{
            .label = TrPad("Extrude pad"),
            .undo =
                [world, part_ptr, wood, factory = ctx.SceneFactory,
                 session = ctx.Session, redraw = ctx.RequestRedraw,
                 refresh = ctx.RefreshUi] {
                    if (!world || !part_ptr)
                    {
                        return;
                    }
                    adapter::WithScene(
                        factory, world->Document(),
                        [](adapter::ISceneService& svc) { svc.UndoFeature(); });
                    Material mat =
                        wood.empty() ? Material{} : MakeWoodMaterial(wood);
                    world->SyncPartBodies(*part_ptr, std::move(mat));
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
                [world, part_ptr, wood, factory = ctx.SceneFactory,
                 session = ctx.Session, redraw = ctx.RequestRedraw,
                 refresh = ctx.RefreshUi] {
                    if (!world || !part_ptr)
                    {
                        return;
                    }
                    adapter::WithScene(
                        factory, world->Document(),
                        [](adapter::ISceneService& svc) { svc.RedoFeature(); });
                    Material mat =
                        wood.empty() ? Material{} : MakeWoodMaterial(wood);
                    world->SyncPartBodies(*part_ptr, std::move(mat));
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

    m_result = CommandResult::Ok(TrPad("Extrude created"));
    m_finished = true;
    if (ctx.RefreshUi)
    {
        ctx.RefreshUi();
    }
}

}  // namespace brep::viewer::commands
