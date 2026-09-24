#include "commands/tools/CreateSphereTool.h"
#include "commands/tools/PreviewEdges.h"

#include "adapter/ISceneServiceFactory.h"
#include "commands/DocumentHistory.h"
#include "commands/snap/Accusnap.h"

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include <QCoreApplication>
#include <QMouseEvent>

#include <cmath>
#include <numbers>

namespace brep::viewer::commands
{
namespace
{

QString TrSphere(const char* source)
{
    return QCoreApplication::translate("CreateSphereTool", source);
}

EdgeMesh MakeSphereWire(const Point3d& c, double r, int seg = 32)
{
  EdgeMesh mesh;
  auto ring = [&](char axis)
  {
    for (int i = 0; i < seg; ++i)
  {
      const double t0 = 2.0 * std::numbers::pi * static_cast<double>(i) / seg;
      const double t1 = 2.0 * std::numbers::pi * static_cast<double>(i + 1) / seg;
      Point3d a = c;
      Point3d b = c;
      if (axis == 'y')
      {
        a = Point3d{c.x() + r * std::cos(t0), c.y(), c.z() + r * std::sin(t0)};
        b = Point3d{c.x() + r * std::cos(t1), c.y(), c.z() + r * std::sin(t1)};
      } else if (axis == 'x')
      {
        a = Point3d{c.x(), c.y() + r * std::cos(t0), c.z() + r * std::sin(t0)};
        b = Point3d{c.x(), c.y() + r * std::cos(t1), c.z() + r * std::sin(t1)};
      }
      else
      {
        a = Point3d{c.x() + r * std::cos(t0), c.y() + r * std::sin(t0), c.z()};
        b = Point3d{c.x() + r * std::cos(t1), c.y() + r * std::sin(t1), c.z()};
      }
      PushSegment(mesh, a, b);
    }
  };
  ring('x');
  ring('y');
  ring('z');
  return mesh;
}

void PushTriangle(TriangleMesh& mesh, const Point3d& a, const Point3d& b,
              const Point3d& c)
{
  const Vector3d n = (b - a).cross(c - a);
  const Vector3d nn = n.norm() > 1e-12 ? n.normalized() : Vector3d{0.0, 1.0, 0.0};
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.Vertices.size());
  mesh.Vertices.push_back(MeshVertex{a, nn, {}});
  mesh.Vertices.push_back(MeshVertex{b, nn, {}});
  mesh.Vertices.push_back(MeshVertex{c, nn, {}});
  mesh.Indices.push_back(base);
  mesh.Indices.push_back(base + 1);
  mesh.Indices.push_back(base + 2);
}

/// Preview solid mesh (procedural UV sphere — no kernel builder).
TriangleMesh MakeSphereSolid(const Point3d& center, double r, int stacks = 16,
                               int slices = 32)
{
  TriangleMesh mesh;
  if (!(r > 0.0)) return mesh;

  auto vertex = [&](double phi, double theta) -> Point3d {
    const double sin_phi = std::sin(phi);
    return Point3d{center.x() + r * sin_phi * std::cos(theta),
                   center.y() + r * std::cos(phi),
                   center.z() + r * sin_phi * std::sin(theta)};
  };

  for (int i = 0; i < stacks; ++i)
  {
    const double phi0 = std::numbers::pi * static_cast<double>(i) / stacks;
    const double phi1 = std::numbers::pi * static_cast<double>(i + 1) / stacks;
    for (int j = 0; j < slices; ++j)
    {
      const double t0 = 2.0 * std::numbers::pi * static_cast<double>(j) / slices;
      const double t1 =
          2.0 * std::numbers::pi * static_cast<double>(j + 1) / slices;
      const Point3d a = vertex(phi0, t0);
      const Point3d b = vertex(phi0, t1);
      const Point3d c = vertex(phi1, t1);
      const Point3d d = vertex(phi1, t0);
      if (i > 0)
      {
        PushTriangle(mesh, a, b, c);
      }
      if (i < stacks - 1)
      {
        PushTriangle(mesh, a, c, d);
      }
    }
  }
  return mesh;
}

}  // namespace

QString CreateSphereTool::Prompt() const
{
  if (m_step == Step::Center)
{
    return TrSphere("Create sphere: pick center on a surface or ground (ESC cancel)");
  }
  return TrSphere("Create sphere: pick radius point (ESC cancel)");
}

void CreateSphereTool::OnStart(CommandContext& ctx)
{
  m_step = Step::Center;
  m_finished = false;
  m_result = CommandResult::Cancelled();
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->LastPoint.reset();
  ClearPreview(ctx);
  if (ctx.ReportStatus) ctx.ReportStatus(Prompt());
}

bool CreateSphereTool::PickPoint(CommandContext& ctx, float x, float y,
                                  Point3d& hit) const
{
  const PickResult result = AccuSnap::Resolve(ctx, x, y);
  if (result.Kind == SnapKind::None) return false;
  hit = result.Point;
  return true;
}

void CreateSphereTool::ClearPreview(CommandContext& ctx)
{
  if (ctx.ClearPreview) ctx.ClearPreview();
}

void CreateSphereTool::UpdatePreview(CommandContext& ctx, float x, float y)
{
  if (m_step != Step::Radius) return;
  Point3d hit;
  if (!PickPoint(ctx, x, y, hit)) return;
  const double radius = (hit - m_center).norm();
  if (radius < 1e-4) return;

  EdgeMesh wire = MakeSphereWire(m_center, radius);
  const EdgeMesh marker = MakePointMarker(m_center);
  wire.Positions.insert(wire.Positions.end(), marker.Positions.begin(),
                        marker.Positions.end());
  TriangleMesh solid = MakeSphereSolid(m_center, radius);
  if (ctx.SetPreview) ctx.SetPreview(std::move(wire), std::move(solid));
  else if (ctx.SetPreviewEdges) ctx.SetPreviewEdges(std::move(wire));
  if (ctx.RequestRedraw) ctx.RequestRedraw();
}

void CreateSphereTool::CommitSphere(CommandContext& ctx, double radius)
{
  ClearPreview(ctx);
  if (!ctx.World || !ctx.World->Document())
  {
    m_result = CommandResult::Failed(TrSphere("No active document"));
    m_finished = true;
    return;
  }

  if (!ctx.Scene)
  {
    m_result = CommandResult::Failed(TrSphere("No main part"));
    m_finished = true;
    return;
  }

  adapter::ISceneService& scene = *ctx.Scene;
  Part* part = scene.MainPart();
  if (!part)
  {
    m_result = CommandResult::Failed(TrSphere("No main part"));
    m_finished = true;
    return;
  }

  SphereSpec spec;
  spec.Center = m_center;
  spec.Radius = radius;
  spec.Name = "sphere";

  Body* body = scene.AddPrimitive(spec);
  if (!body)
  {
    m_result = CommandResult::Failed(TrSphere("Failed to create sphere"));
    m_finished = true;
    return;
  }

  const feat::FeatureId fid =
      scene.FeatureIdFor(Guid{}, body->Guid).value_or(feat::FeatureId{});
  if (fid.IsValid())
  {
    scene.RecordAppendPrimitive(fid, spec);
  }
  const Guid feature_guid = fid.Guid;

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
        .label = TrSphere("Create sphere"),
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
      TrSphere("Created sphere %1")
          .arg(QString::fromStdString(guid.ToString())));
  m_finished = true;
  if (ctx.RefreshUi) ctx.RefreshUi();
}

bool CreateSphereTool::OnMousePress(CommandContext& ctx, float x, float y,
                                      int button)
{
  if (button != Qt::LeftButton) return false;

  Point3d hit;
  if (!PickPoint(ctx, x, y, hit))
  {
    if (ctx.ReportStatus)
  {
      ctx.ReportStatus(
          TrSphere("Missed surface/ground — try another angle"));
    }
    return true;
  }
  if (m_step == Step::Center)
  {
    m_center = hit;
    if (ctx.SnapSessionRef) ctx.SnapSessionRef->LastPoint = hit;
    m_step = Step::Radius;
    if (ctx.SetPreviewEdges)
    {
      ctx.SetPreviewEdges(MakePointMarker(m_center));
    }
    if (ctx.RequestRedraw) ctx.RequestRedraw();
    if (ctx.ReportStatus) ctx.ReportStatus(Prompt());
    return true;
  }

  const double radius = (hit - m_center).norm();
  if (radius < 1e-4)
  {
    if (ctx.ReportStatus)
  {
      ctx.ReportStatus(TrSphere("Radius too small — pick farther"));
    }
    return true;
  }
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->LastPoint = hit;
  CommitSphere(ctx, radius);
  return true;
}

void CreateSphereTool::OnMouseMove(CommandContext& ctx, float x, float y)
{
  if (m_step == Step::Center)
{
    Point3d hover;
    (void)PickPoint(ctx, x, y, hover);
  } else if (m_step == Step::Radius)
  {
    UpdatePreview(ctx, x, y);
  }
}

void CreateSphereTool::OnCancel(CommandContext& ctx)
{
  ClearPreview(ctx);
  m_finished = true;
  m_result = CommandResult::Cancelled(TrSphere("Cancelled create sphere"));
  if (ctx.ReportStatus) ctx.ReportStatus(m_result.Message);
}

}  // namespace brep::viewer::commands
