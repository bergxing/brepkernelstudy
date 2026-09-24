#include "commands/tools/CreateBoxTool.h"
#include "commands/tools/PreviewEdges.h"

#include "adapter/ISceneServiceFactory.h"
#include "commands/DocumentHistory.h"
#include "commands/Picking.h"
#include "commands/snap/Accusnap.h"

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include <QCoreApplication>
#include <QMouseEvent>

#include <algorithm>
#include <cmath>

namespace brep::viewer::commands
{
namespace
{

QString TrBox(const char* source)
{
    return QCoreApplication::translate("CreateBoxTool", source);
}

EdgeMesh MakeRectWire(double minx, double minz, double maxx, double maxz,
                      double y)
{
  EdgeMesh mesh;
  const Point3d p00{minx, y, minz};
  const Point3d p10{maxx, y, minz};
  const Point3d p11{maxx, y, maxz};
  const Point3d p01{minx, y, maxz};
  PushSegment(mesh, p00, p10);
  PushSegment(mesh, p10, p11);
  PushSegment(mesh, p11, p01);
  PushSegment(mesh, p01, p00);
  return mesh;
}

EdgeMesh MakeBoxWire(double minx, double miny, double minz, double maxx,
                       double maxy, double maxz)
{
  EdgeMesh mesh;
  const Point3d p000{minx, miny, minz};
  const Point3d p100{maxx, miny, minz};
  const Point3d p110{maxx, miny, maxz};
  const Point3d p010{minx, miny, maxz};
  const Point3d p001{minx, maxy, minz};
  const Point3d p101{maxx, maxy, minz};
  const Point3d p111{maxx, maxy, maxz};
  const Point3d p011{minx, maxy, maxz};
  // bottom
  PushSegment(mesh, p000, p100);
  PushSegment(mesh, p100, p110);
  PushSegment(mesh, p110, p010);
  PushSegment(mesh, p010, p000);
  // top
  PushSegment(mesh, p001, p101);
  PushSegment(mesh, p101, p111);
  PushSegment(mesh, p111, p011);
  PushSegment(mesh, p011, p001);
  // verticals
  PushSegment(mesh, p000, p001);
  PushSegment(mesh, p100, p101);
  PushSegment(mesh, p110, p111);
  PushSegment(mesh, p010, p011);
  return mesh;
}

void PushTriangle(TriangleMesh& mesh, const Point3d& a, const Point3d& b,
              const Point3d& c, const Vector3d& n)
{
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.Vertices.size());
  mesh.Vertices.push_back(MeshVertex{a, n, {}});
  mesh.Vertices.push_back(MeshVertex{b, n, {}});
  mesh.Vertices.push_back(MeshVertex{c, n, {}});
  mesh.Indices.push_back(base);
  mesh.Indices.push_back(base + 1);
  mesh.Indices.push_back(base + 2);
}

TriangleMesh MakeRectSolid(double minx, double minz, double maxx, double maxz,
                             double y)
{
  TriangleMesh mesh;
  const Point3d p00{minx, y, minz};
  const Point3d p10{maxx, y, minz};
  const Point3d p11{maxx, y, maxz};
  const Point3d p01{minx, y, maxz};
  const Vector3d n{0.0, 1.0, 0.0};
  PushTriangle(mesh, p00, p10, p11, n);
  PushTriangle(mesh, p00, p11, p01, n);
  return mesh;
}

TriangleMesh MakeBoxSolid(double minx, double miny, double minz, double maxx,
                            double maxy, double maxz)
{
  TriangleMesh mesh;
  const Point3d p000{minx, miny, minz};
  const Point3d p100{maxx, miny, minz};
  const Point3d p110{maxx, miny, maxz};
  const Point3d p010{minx, miny, maxz};
  const Point3d p001{minx, maxy, minz};
  const Point3d p101{maxx, maxy, minz};
  const Point3d p111{maxx, maxy, maxz};
  const Point3d p011{minx, maxy, maxz};
  // -Y / +Y
  PushTriangle(mesh, p000, p100, p110, Vector3d{0.0, -1.0, 0.0});
  PushTriangle(mesh, p000, p110, p010, Vector3d{0.0, -1.0, 0.0});
  PushTriangle(mesh, p001, p011, p111, Vector3d{0.0, 1.0, 0.0});
  PushTriangle(mesh, p001, p111, p101, Vector3d{0.0, 1.0, 0.0});
  // -Z / +Z
  PushTriangle(mesh, p000, p001, p101, Vector3d{0.0, 0.0, -1.0});
  PushTriangle(mesh, p000, p101, p100, Vector3d{0.0, 0.0, -1.0});
  PushTriangle(mesh, p010, p110, p111, Vector3d{0.0, 0.0, 1.0});
  PushTriangle(mesh, p010, p111, p011, Vector3d{0.0, 0.0, 1.0});
  // -X / +X
  PushTriangle(mesh, p000, p010, p011, Vector3d{-1.0, 0.0, 0.0});
  PushTriangle(mesh, p000, p011, p001, Vector3d{-1.0, 0.0, 0.0});
  PushTriangle(mesh, p100, p101, p111, Vector3d{1.0, 0.0, 0.0});
  PushTriangle(mesh, p100, p111, p110, Vector3d{1.0, 0.0, 0.0});
  return mesh;
}

void BaseBounds(const Point3d& a, const Point3d& b, double& minx, double& maxx,
                 double& minz, double& maxz)
{
  minx = std::min(a.x(), b.x());
  maxx = std::max(a.x(), b.x());
  minz = std::min(a.z(), b.z());
  maxz = std::max(a.z(), b.z());
}

}  // namespace

QString CreateBoxTool::Prompt() const
{
    switch (m_step)
    {
        case Step::FirstCorner:
            return TrBox("Create box: pick first bottom corner (ESC cancel)");
        case Step::OppositeCorner:
            return TrBox(
                "Create box: pick opposite bottom corner (ESC cancel)");
        case Step::Height:
            return TrBox("Create box: pick height (ESC cancel)");
    }
    return {};
}

void CreateBoxTool::ClearPreview(CommandContext& ctx)
{
  if (ctx.ClearPreview) ctx.ClearPreview();
}

void CreateBoxTool::OnStart(CommandContext& ctx)
{
  m_step = Step::FirstCorner;
  m_finished = false;
  m_result = CommandResult::Cancelled();
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->LastPoint.reset();
  ClearPreview(ctx);
  if (ctx.ReportStatus) ctx.ReportStatus(Prompt());
  BREP_INFO("CreateBoxTool start (3-point: base + height)");
}

bool CreateBoxTool::PickGround(CommandContext& ctx, float x, float y,
                                Point3d& hit) const
{
  const PickResult result = AccuSnap::Resolve(ctx, x, y);
  if (result.Kind == SnapKind::None) return false;
  hit = result.Point;
  return true;
}

bool CreateBoxTool::PickHeight(CommandContext& ctx, float x, float y,
                                double& height) const
{
  Camera* cam = ctx.ViewCamera
                    ? ctx.ViewCamera
                    : (ctx.World ? ctx.World->MainCamera() : nullptr);
  if (!cam) return false;

  Point3d origin;
  Vector3d dir;
  if (!ScreenToRay(*cam, ctx.ViewportWidth, ctx.ViewportHeight, x, y, origin, dir))
  {
    return false;
  }

  double minx = 0, maxx = 0, minz = 0, maxz = 0;
  BaseBounds(m_cornerA, m_cornerB, minx, maxx, minz, maxz);
  const Point3d pivot{(minx + maxx) * 0.5, 0.0, (minz + maxz) * 0.5};

  // Vertical plane through base center, facing the camera.
  const Point3d eye = cam->eye();
  Vector3d n{eye.x() - pivot.x(), 0.0, eye.z() - pivot.z()};
  if (n.norm() < 1e-6) n = Vector3d{1.0, 0.0, 0.0};
  n = n.normalized();

  Point3d hit;
  if (!IntersectPlane(origin, dir, pivot, n, hit)) return false;
  height = hit.y();
  return true;
}

void CreateBoxTool::UpdatePreview(CommandContext& ctx, float x, float y)
{
  if (!ctx.SetPreview && !ctx.SetPreviewEdges)
{
    BREP_WARN("CreateBoxTool: preview callback is empty");
    return;
  }
  const auto pushPreview = [&](EdgeMesh wire, TriangleMesh solid)
  {
    if (ctx.SetPreview)
  {
      ctx.SetPreview(std::move(wire), std::move(solid));
    }
    else
  {
      ctx.SetPreviewEdges(std::move(wire));
    }
  };

  if (m_step == Step::OppositeCorner)
  {
    Point3d hit;
    if (!PickGround(ctx, x, y, hit))
    {
      // Keep the first-point marker visible even if the ray misses.
      pushPreview(MakePointMarker(m_cornerA), {});
      if (ctx.RequestRedraw) ctx.RequestRedraw();
      return;
    }
    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    BaseBounds(m_cornerA, hit, minx, maxx, minz, maxz);
    EdgeMesh wire = MakeRectWire(minx, minz, maxx, maxz, 0.0);
    // Also keep a marker on the first corner.
    EdgeMesh marker = MakePointMarker(m_cornerA);
    wire.Positions.insert(wire.Positions.end(), marker.Positions.begin(),
                          marker.Positions.end());
    pushPreview(std::move(wire), MakeRectSolid(minx, minz, maxx, maxz, 0.0));
    if (ctx.RequestRedraw) ctx.RequestRedraw();
    return;
  }

  if (m_step == Step::Height)
  {
    double height = 0.0;
    if (!PickHeight(ctx, x, y, height)) return;
    if (std::abs(height) < 1e-4) height = (height < 0.0) ? -1e-3 : 1e-3;
    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    BaseBounds(m_cornerA, m_cornerB, minx, maxx, minz, maxz);
    const double miny = std::min(0.0, height);
    const double maxy = std::max(0.0, height);
    pushPreview(MakeBoxWire(minx, miny, minz, maxx, maxy, maxz),
                 MakeBoxSolid(minx, miny, minz, maxx, maxy, maxz));
    if (ctx.RequestRedraw) ctx.RequestRedraw();
  }
}

void CreateBoxTool::CommitBox(CommandContext& ctx, double height)
{
  using namespace brep;
  ClearPreview(ctx);

  if (!ctx.Scene)
  {
    m_result = CommandResult::Failed(TrBox("No active document"));
    m_finished = true;
    return;
  }
  adapter::ISceneService& scene = *ctx.Scene;
  Part* part = scene.MainPart();
  if (!part)
  {
    m_result = CommandResult::Failed(TrBox("No main part"));
    m_finished = true;
    return;
  }

  double minx = 0, maxx = 0, minz = 0, maxz = 0;
  BaseBounds(m_cornerA, m_cornerB, minx, maxx, minz, maxz);
  if (std::abs(maxx - minx) < 1e-4 || std::abs(maxz - minz) < 1e-4 ||
      std::abs(height) < 1e-4)
  {
    m_result = CommandResult::Failed(
        TrBox("Box too small — pick a larger base or height"));
    m_finished = true;
    return;
  }

  const double miny = std::min(0.0, height);
  const double maxy = std::max(0.0, height);

  BREP_INFO(
      "CreateBoxTool commit extents min=({:.4f},{:.4f},{:.4f}) "
      "max=({:.4f},{:.4f},{:.4f})",
      minx, miny, minz, maxx, maxy, maxz);

  BoxSpec spec{
      .Min = Point3d{minx, miny, minz},
      .Max = Point3d{maxx, maxy, maxz},
      .Name = "box",
  };
  Body* body = scene.AddPrimitive(spec);
  if (!body)
  {
    m_result = CommandResult::Failed(TrBox("Failed to create box"));
    m_finished = true;
    return;
  }

  Guid feature_guid{};
  if (auto obj = scene.ObjectForBody(body->Guid))
  {
    feature_guid = obj->FeatureGuid;
    scene.RecordAppendPrimitive(feat::FeatureId{feature_guid}, spec);
  }

  Material material = ctx.WoodAlbedoPath.empty()
                          ? Material{}
                          : MakeWoodMaterial(ctx.WoodAlbedoPath);
  auto mesh = scene.MeshForBody(body->Guid);
  ctx.World->CreateBodyRenderable(body->Name, body->Guid,
                                    std::move(mesh.Faces),
                                    std::move(mesh.Edges), material, Point3d{},
                                    feature_guid);
  if (ctx.RequestRedraw) ctx.RequestRedraw();
  if (ctx.Session) ctx.Session->MarkDirty();

  const Guid guid = body->Guid;
  const std::string wood = ctx.WoodAlbedoPath;
  ecs::World* world = ctx.World;
  Part* part_ptr = part;

  if (ctx.History)
  {
    ctx.History->push(DocumentHistory::Entry{
        .label = TrBox("Create box"),
        .undo =
            [world, part_ptr, wood, factory = ctx.SceneFactory,
             session = ctx.Session, redraw = ctx.RequestRedraw,
             refresh = ctx.RefreshUi] {
              if (!world || !part_ptr) return;
              adapter::WithScene(
                  factory, world->Document(),
                  [](adapter::ISceneService& scene) { scene.UndoFeature(); });
              Material material =
                  wood.empty() ? Material{} : MakeWoodMaterial(wood);
              world->SyncPartBodies(*part_ptr, std::move(material));
              if (session) session->MarkDirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
        .redo =
            [world, part_ptr, wood, factory = ctx.SceneFactory,
             session = ctx.Session, redraw = ctx.RequestRedraw,
             refresh = ctx.RefreshUi] {
              if (!world || !part_ptr) return;
              adapter::WithScene(
                  factory, world->Document(),
                  [](adapter::ISceneService& scene) { scene.RedoFeature(); });
              Material material =
                  wood.empty() ? Material{} : MakeWoodMaterial(wood);
              world->SyncPartBodies(*part_ptr, std::move(material));
              if (session) session->MarkDirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
    });
  }

  m_result = CommandResult::Ok(
      TrBox("Created box %1").arg(QString::fromStdString(guid.ToString())));
  m_finished = true;
  if (ctx.RefreshUi) ctx.RefreshUi();
  BREP_INFO("CreateBoxTool committed guid={}", guid.ToString());
}

bool CreateBoxTool::OnMousePress(CommandContext& ctx, float x, float y,
                                   int button)
{
  if (button != Qt::LeftButton) return false;

  BREP_INFO("CreateBoxTool click screen=({:.1f},{:.1f}) step={} viewport={}x{}",
            x, y, static_cast<int>(m_step), ctx.ViewportWidth,
            ctx.ViewportHeight);

  if (m_step == Step::FirstCorner || m_step == Step::OppositeCorner)
  {
    Point3d hit;
    if (!PickGround(ctx, x, y, hit))
    {
      BREP_WARN("CreateBoxTool pick ground failed at screen=({:.1f},{:.1f})", x,
                y);
      if (ctx.ReportStatus)
      {
        ctx.ReportStatus(TrBox("Missed surface/ground — try another angle"));
      }
      return true;
    }
    BREP_INFO("CreateBoxTool picked ground point=({:.4f},{:.4f},{:.4f}) step={}",
              hit.x(), hit.y(), hit.z(), static_cast<int>(m_step));

    if (m_step == Step::FirstCorner)
    {
      m_cornerA = hit;
      if (ctx.SnapSessionRef) ctx.SnapSessionRef->LastPoint = hit;
      m_step = Step::OppositeCorner;
      // Immediate feedback before the next move arrives.
      if (ctx.SetPreviewEdges)
      {
        ctx.SetPreviewEdges(MakePointMarker(m_cornerA));
      }
      if (ctx.RequestRedraw) ctx.RequestRedraw();
      if (ctx.ReportStatus) ctx.ReportStatus(Prompt());
      return true;
    }

    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    BaseBounds(m_cornerA, hit, minx, maxx, minz, maxz);
    if (std::abs(maxx - minx) < 1e-4 || std::abs(maxz - minz) < 1e-4)
    {
      BREP_WARN("CreateBoxTool base too small dx={:.6f} dz={:.6f}",
                maxx - minx, maxz - minz);
      if (ctx.ReportStatus)
      {
        ctx.ReportStatus(TrBox("Base too small — pick farther"));
      }
      return true;
    }
    if (ctx.SnapSessionRef) ctx.SnapSessionRef->LastPoint = hit;
    m_cornerB = hit;
    m_step = Step::Height;
    AccuSnap::ClearFeedback(ctx);
    UpdatePreview(ctx, x, y);
    if (ctx.ReportStatus) ctx.ReportStatus(Prompt());
    return true;
  }

  double height = 0.0;
  if (!PickHeight(ctx, x, y, height))
  {
    BREP_WARN("CreateBoxTool pick height failed at screen=({:.1f},{:.1f})", x,
              y);
    if (ctx.ReportStatus)
    {
      ctx.ReportStatus(TrBox("Could not pick height — try another angle"));
    }
    return true;
  }
  BREP_INFO("CreateBoxTool picked height={:.4f}", height);
  CommitBox(ctx, height);
  return true;
}

void CreateBoxTool::OnMouseMove(CommandContext& ctx, float x, float y)
{
  if (m_step == Step::FirstCorner)
{
    Point3d hover;
    (void)PickGround(ctx, x, y, hover);
  } else if (m_step == Step::OppositeCorner || m_step == Step::Height)
  {
    UpdatePreview(ctx, x, y);
  }
}

void CreateBoxTool::OnCancel(CommandContext& ctx)
{
  ClearPreview(ctx);
  m_finished = true;
  m_result = CommandResult::Cancelled(TrBox("Cancelled create box"));
  if (ctx.ReportStatus) ctx.ReportStatus(m_result.Message);
  BREP_INFO("CreateBoxTool cancelled at step={}", static_cast<int>(m_step));
}

}  // namespace brep::viewer::commands
