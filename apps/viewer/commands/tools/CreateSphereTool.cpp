#include "commands/tools/CreateSphereTool.h"

#include "adapter/SceneAdapter.h"
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

QString tr_sphere(const char* source)
{
  return QCoreApplication::translate("CreateSphereTool", source);
}

void push_seg(EdgeMesh& mesh, const Point3d& a, const Point3d& b)
{
  mesh.Positions.push_back(a);
  mesh.Positions.push_back(b);
}

EdgeMesh make_point_marker(const Point3d& p, double s = 0.12)
{
  EdgeMesh mesh;
  push_seg(mesh, Point3d{p.x() - s, p.y(), p.z()},
           Point3d{p.x() + s, p.y(), p.z()});
  push_seg(mesh, Point3d{p.x(), p.y(), p.z() - s},
           Point3d{p.x(), p.y(), p.z() + s});
  push_seg(mesh, Point3d{p.x(), p.y() - s, p.z()},
           Point3d{p.x(), p.y() + s, p.z()});
  return mesh;
}

EdgeMesh make_sphere_wire(const Point3d& c, double r, int seg = 32)
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
      push_seg(mesh, a, b);
    }
  };
  ring('x');
  ring('y');
  ring('z');
  return mesh;
}

/// Preview solid uses the same analytic body + deflection tessellation as commit.
TriangleMesh make_sphere_solid(const Point3d& center, double r)
{
  if (!(r > 0.0)) return {};
  Model model;
  Body* body = MakeSphere(model, SphereSpec{.Center = center, .Radius = r,
                                             .Name = "preview"});
  if (!body) return {};
  return TessellateBody(*body, TessellationOptions::ForRadius(r));
}

}  // namespace

QString CreateSphereTool::prompt() const
{
  if (m_step == 0)
{
    return tr_sphere("Create sphere: pick center on a surface or ground (ESC cancel)");
  }
  return tr_sphere("Create sphere: pick radius point (ESC cancel)");
}

void CreateSphereTool::on_start(CommandContext& ctx)
{
  m_step = 0;
  m_finished = false;
  m_result = CommandResult::Cancelled();
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point.reset();
  clear_preview(ctx);
  if (ctx.ReportStatus) ctx.ReportStatus(prompt());
}

bool CreateSphereTool::pick_point(CommandContext& ctx, float x, float y,
                                  Point3d& hit) const
{
  const PickResult result = AccuSnap::resolve(ctx, x, y);
  if (result.kind == SnapKind::None) return false;
  hit = result.point;
  return true;
}

void CreateSphereTool::clear_preview(CommandContext& ctx)
{
  if (ctx.ClearPreview) ctx.ClearPreview();
}

void CreateSphereTool::update_preview(CommandContext& ctx, float x, float y)
{
  if (m_step != 1) return;
  Point3d hit;
  if (!pick_point(ctx, x, y, hit)) return;
  const double radius = (hit - m_center).norm();
  if (radius < 1e-4) return;

  EdgeMesh wire = make_sphere_wire(m_center, radius);
  const EdgeMesh marker = make_point_marker(m_center);
  wire.Positions.insert(wire.Positions.end(), marker.Positions.begin(),
                        marker.Positions.end());
  TriangleMesh solid = make_sphere_solid(m_center, radius);
  if (ctx.SetPreview) ctx.SetPreview(std::move(wire), std::move(solid));
  else if (ctx.SetPreviewEdges) ctx.SetPreviewEdges(std::move(wire));
  if (ctx.RequestRedraw) ctx.RequestRedraw();
}

void CreateSphereTool::commit_sphere(CommandContext& ctx, double radius)
{
  clear_preview(ctx);
  if (!ctx.World || !ctx.World->document())
  {
    m_result = CommandResult::Failed(tr_sphere("No active document"));
    m_finished = true;
    return;
  }

  adapter::SceneAdapter scene(ctx.World->document());
  Part* part = scene.main_part();
  if (!part)
  {
    m_result = CommandResult::Failed(tr_sphere("No main part"));
    m_finished = true;
    return;
  }

  SphereSpec spec;
  spec.Center = m_center;
  spec.Radius = radius;
  spec.Name = "sphere";

  Body* body = scene.add_sphere(spec);
  if (!body)
  {
    m_result = CommandResult::Failed(tr_sphere("Failed to create sphere"));
    m_finished = true;
    return;
  }

  const feat::FeatureId fid =
      scene.feature_id_for(Guid{}, body->Guid).value_or(feat::FeatureId{});
  if (fid.IsValid())
  {
    scene.record_append_sphere(fid, spec);
  }
  const Guid feature_guid = fid.Guid;

  Material material = ctx.WoodAlbedoPath.empty()
                          ? Material{}
                          : MakeWoodMaterial(ctx.WoodAlbedoPath);
  auto mesh = scene.mesh_for_body(body->Guid);
  ctx.World->create_body_renderable(body->Name, body->Guid,
                                    std::move(mesh.faces),
                                    std::move(mesh.edges), material, Point3d{},
                                    feature_guid);
  if (ctx.RequestRedraw) ctx.RequestRedraw();
  if (ctx.Session) ctx.Session->mark_dirty();

  const Guid guid = body->Guid;
  const std::string wood = ctx.WoodAlbedoPath;
  ecs::World* world = ctx.World;
  Part* part_ptr = part;

  if (ctx.History)
  {
    ctx.History->push(DocumentHistory::Entry{
        .label = tr_sphere("Create sphere"),
        .undo =
            [world, part_ptr, wood, session = ctx.Session,
             redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
              if (!world || !part_ptr) return;
              adapter::SceneAdapter scene_u(world->document());
              scene_u.undo_feature();
              Material material =
                  wood.empty() ? Material{} : MakeWoodMaterial(wood);
              world->sync_part_bodies(*part_ptr, std::move(material));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
        .redo =
            [world, part_ptr, wood, session = ctx.Session,
             redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
              if (!world || !part_ptr) return;
              adapter::SceneAdapter scene_r(world->document());
              scene_r.redo_feature();
              Material material =
                  wood.empty() ? Material{} : MakeWoodMaterial(wood);
              world->sync_part_bodies(*part_ptr, std::move(material));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
    });
  }

  m_result = CommandResult::Ok(
      tr_sphere("Created sphere %1")
          .arg(QString::fromStdString(guid.ToString())));
  m_finished = true;
  if (ctx.RefreshUi) ctx.RefreshUi();
}

bool CreateSphereTool::on_mouse_press(CommandContext& ctx, float x, float y,
                                      int button)
{
  if (button != Qt::LeftButton) return false;

  Point3d hit;
  if (!pick_point(ctx, x, y, hit))
  {
    if (ctx.ReportStatus)
  {
      ctx.ReportStatus(
          tr_sphere("Missed surface/ground — try another angle"));
    }
    return true;
  }
  if (m_step == 0)
  {
    m_center = hit;
    if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point = hit;
    m_step = 1;
    if (ctx.SetPreviewEdges)
    {
      ctx.SetPreviewEdges(make_point_marker(m_center));
    }
    if (ctx.RequestRedraw) ctx.RequestRedraw();
    if (ctx.ReportStatus) ctx.ReportStatus(prompt());
    return true;
  }

  const double radius = (hit - m_center).norm();
  if (radius < 1e-4)
  {
    if (ctx.ReportStatus)
  {
      ctx.ReportStatus(tr_sphere("Radius too small — pick farther"));
    }
    return true;
  }
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point = hit;
  commit_sphere(ctx, radius);
  return true;
}

void CreateSphereTool::on_mouse_move(CommandContext& ctx, float x, float y)
{
  if (m_step == 0)
{
    Point3d hover;
    (void)pick_point(ctx, x, y, hover);
  } else if (m_step == 1)
  {
    update_preview(ctx, x, y);
  }
}

void CreateSphereTool::on_cancel(CommandContext& ctx)
{
  clear_preview(ctx);
  m_finished = true;
  m_result = CommandResult::Cancelled(tr_sphere("Cancelled create sphere"));
  if (ctx.ReportStatus) ctx.ReportStatus(m_result.Message);
}

}  // namespace brep::viewer::commands
