#include "commands/tools/CreateBoxTool.h"

#include "adapter/SceneAdapter.h"
#include "commands/DocumentHistory.h"
#include "commands/Picking.h"
#include "commands/snap/Accusnap.h"

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include <QMouseEvent>

#include <algorithm>
#include <cmath>

namespace brep::viewer::commands
{
namespace
{

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

EdgeMesh make_rect_wire(double minx, double minz, double maxx, double maxz,
                        double y)
{
  EdgeMesh mesh;
  const Point3d p00{minx, y, minz};
  const Point3d p10{maxx, y, minz};
  const Point3d p11{maxx, y, maxz};
  const Point3d p01{minx, y, maxz};
  push_seg(mesh, p00, p10);
  push_seg(mesh, p10, p11);
  push_seg(mesh, p11, p01);
  push_seg(mesh, p01, p00);
  return mesh;
}

EdgeMesh make_box_wire(double minx, double miny, double minz, double maxx,
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
  push_seg(mesh, p000, p100);
  push_seg(mesh, p100, p110);
  push_seg(mesh, p110, p010);
  push_seg(mesh, p010, p000);
  // top
  push_seg(mesh, p001, p101);
  push_seg(mesh, p101, p111);
  push_seg(mesh, p111, p011);
  push_seg(mesh, p011, p001);
  // verticals
  push_seg(mesh, p000, p001);
  push_seg(mesh, p100, p101);
  push_seg(mesh, p110, p111);
  push_seg(mesh, p010, p011);
  return mesh;
}

void push_tri(TriangleMesh& mesh, const Point3d& a, const Point3d& b,
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

TriangleMesh make_rect_solid(double minx, double minz, double maxx, double maxz,
                             double y)
{
  TriangleMesh mesh;
  const Point3d p00{minx, y, minz};
  const Point3d p10{maxx, y, minz};
  const Point3d p11{maxx, y, maxz};
  const Point3d p01{minx, y, maxz};
  const Vector3d n{0.0, 1.0, 0.0};
  push_tri(mesh, p00, p10, p11, n);
  push_tri(mesh, p00, p11, p01, n);
  return mesh;
}

TriangleMesh make_box_solid(double minx, double miny, double minz, double maxx,
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
  push_tri(mesh, p000, p100, p110, Vector3d{0.0, -1.0, 0.0});
  push_tri(mesh, p000, p110, p010, Vector3d{0.0, -1.0, 0.0});
  push_tri(mesh, p001, p011, p111, Vector3d{0.0, 1.0, 0.0});
  push_tri(mesh, p001, p111, p101, Vector3d{0.0, 1.0, 0.0});
  // -Z / +Z
  push_tri(mesh, p000, p001, p101, Vector3d{0.0, 0.0, -1.0});
  push_tri(mesh, p000, p101, p100, Vector3d{0.0, 0.0, -1.0});
  push_tri(mesh, p010, p110, p111, Vector3d{0.0, 0.0, 1.0});
  push_tri(mesh, p010, p111, p011, Vector3d{0.0, 0.0, 1.0});
  // -X / +X
  push_tri(mesh, p000, p010, p011, Vector3d{-1.0, 0.0, 0.0});
  push_tri(mesh, p000, p011, p001, Vector3d{-1.0, 0.0, 0.0});
  push_tri(mesh, p100, p101, p111, Vector3d{1.0, 0.0, 0.0});
  push_tri(mesh, p100, p111, p110, Vector3d{1.0, 0.0, 0.0});
  return mesh;
}

void base_bounds(const Point3d& a, const Point3d& b, double& minx, double& maxx,
                 double& minz, double& maxz)
{
  minx = std::min(a.x(), b.x());
  maxx = std::max(a.x(), b.x());
  minz = std::min(a.z(), b.z());
  maxz = std::max(a.z(), b.z());
}

}  // namespace

QString CreateBoxTool::prompt() const
{
  switch (m_step)
{
    case 0:
      return QStringLiteral("创建立方体: 拾取底面第一个角点 (ESC 取消)");
    case 1:
      return QStringLiteral("创建立方体: 拾取底面对角点 (ESC 取消)");
    default:
      return QStringLiteral("创建立方体: 拾取高度点 (ESC 取消)");
  }
}

void CreateBoxTool::clear_preview(CommandContext& ctx)
{
  if (ctx.ClearPreview) ctx.ClearPreview();
}

void CreateBoxTool::on_start(CommandContext& ctx)
{
  m_step = 0;
  m_finished = false;
  m_result = CommandResult::Cancelled();
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point.reset();
  clear_preview(ctx);
  if (ctx.ReportStatus) ctx.ReportStatus(prompt());
  BREP_INFO("CreateBoxTool start (3-point: base + height)");
}

bool CreateBoxTool::pick_ground(CommandContext& ctx, float x, float y,
                                Point3d& hit) const
{
  const PickResult result = AccuSnap::resolve(ctx, x, y);
  if (result.kind == SnapKind::None) return false;
  hit = result.point;
  return true;
}

bool CreateBoxTool::pick_height(CommandContext& ctx, float x, float y,
                                double& height) const
{
  Camera* cam = ctx.ViewCamera
                    ? ctx.ViewCamera
                    : (ctx.World ? ctx.World->main_camera() : nullptr);
  if (!cam) return false;

  Point3d origin;
  Vector3d dir;
  if (!screen_to_ray(*cam, ctx.ViewportWidth, ctx.ViewportHeight, x, y, origin, dir))
  {
    return false;
  }

  double minx = 0, maxx = 0, minz = 0, maxz = 0;
  base_bounds(m_cornerA, m_cornerB, minx, maxx, minz, maxz);
  const Point3d pivot{(minx + maxx) * 0.5, 0.0, (minz + maxz) * 0.5};

  // Vertical plane through base center, facing the camera.
  const Point3d eye = cam->eye();
  Vector3d n{eye.x() - pivot.x(), 0.0, eye.z() - pivot.z()};
  if (n.norm() < 1e-6) n = Vector3d{1.0, 0.0, 0.0};
  n = n.normalized();

  Point3d hit;
  if (!intersect_plane(origin, dir, pivot, n, hit)) return false;
  height = hit.y();
  return true;
}

void CreateBoxTool::update_preview(CommandContext& ctx, float x, float y)
{
  if (!ctx.SetPreview && !ctx.SetPreviewEdges)
{
    BREP_WARN("CreateBoxTool: preview callback is empty");
    return;
  }
  const auto push_preview = [&](EdgeMesh wire, TriangleMesh solid)
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

  if (m_step == 1)
  {
    Point3d hit;
    if (!pick_ground(ctx, x, y, hit))
    {
      // Keep the first-point marker visible even if the ray misses.
      push_preview(make_point_marker(m_cornerA), {});
      if (ctx.RequestRedraw) ctx.RequestRedraw();
      return;
    }
    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    base_bounds(m_cornerA, hit, minx, maxx, minz, maxz);
    EdgeMesh wire = make_rect_wire(minx, minz, maxx, maxz, 0.0);
    // Also keep a marker on the first corner.
    EdgeMesh marker = make_point_marker(m_cornerA);
    wire.Positions.insert(wire.Positions.end(), marker.Positions.begin(),
                          marker.Positions.end());
    push_preview(std::move(wire), make_rect_solid(minx, minz, maxx, maxz, 0.0));
    if (ctx.RequestRedraw) ctx.RequestRedraw();
    return;
  }

  if (m_step == 2)
  {
    double height = 0.0;
    if (!pick_height(ctx, x, y, height)) return;
    if (std::abs(height) < 1e-4) height = (height < 0.0) ? -1e-3 : 1e-3;
    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    base_bounds(m_cornerA, m_cornerB, minx, maxx, minz, maxz);
    const double miny = std::min(0.0, height);
    const double maxy = std::max(0.0, height);
    push_preview(make_box_wire(minx, miny, minz, maxx, maxy, maxz),
                 make_box_solid(minx, miny, minz, maxx, maxy, maxz));
    if (ctx.RequestRedraw) ctx.RequestRedraw();
  }
}

void CreateBoxTool::commit_box(CommandContext& ctx, double height)
{
  using namespace brep;
  clear_preview(ctx);

  adapter::SceneAdapter scene(ctx.World->document());
  Part* part = scene.main_part();
  if (!part)
  {
    m_result = CommandResult::Failed(QStringLiteral("当前没有 Part"));
    m_finished = true;
    return;
  }

  double minx = 0, maxx = 0, minz = 0, maxz = 0;
  base_bounds(m_cornerA, m_cornerB, minx, maxx, minz, maxz);
  if (std::abs(maxx - minx) < 1e-4 || std::abs(maxz - minz) < 1e-4 ||
      std::abs(height) < 1e-4)
  {
    m_result = CommandResult::Failed(QStringLiteral("盒子尺寸过小，请重新拾取"));
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
  Body* body = scene.add_box(spec);
  if (!body)
  {
    m_result = CommandResult::Failed(QStringLiteral("创建盒子失败（再生错误）"));
    m_finished = true;
    return;
  }

  Guid feature_guid{};
  if (auto obj = scene.object_for_body(body->Guid))
  {
    feature_guid = obj->feature_guid;
    scene.record_append_feature(feat::FeatureId{feature_guid}, spec);
  }

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
        .label = QStringLiteral("创建盒子"),
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
      QStringLiteral("已创建盒子 %1")
          .arg(QString::fromStdString(guid.ToString())));
  m_finished = true;
  if (ctx.RefreshUi) ctx.RefreshUi();
  BREP_INFO("CreateBoxTool committed guid={}", guid.ToString());
}

bool CreateBoxTool::on_mouse_press(CommandContext& ctx, float x, float y,
                                   int button)
{
  if (button != Qt::LeftButton) return false;

  BREP_INFO("CreateBoxTool click screen=({:.1f},{:.1f}) step={} viewport={}x{}",
            x, y, m_step, ctx.ViewportWidth, ctx.ViewportHeight);

  if (m_step == 0 || m_step == 1)
  {
    Point3d hit;
    if (!pick_ground(ctx, x, y, hit))
    {
      BREP_WARN("CreateBoxTool pick ground failed at screen=({:.1f},{:.1f})", x,
                y);
      if (ctx.ReportStatus)
      {
        ctx.ReportStatus(QStringLiteral("未点到地面 (y=0)，请换个角度再试"));
      }
      return true;
    }
    BREP_INFO("CreateBoxTool picked ground point=({:.4f},{:.4f},{:.4f}) step={}",
              hit.x(), hit.y(), hit.z(), m_step);

    if (m_step == 0)
    {
      m_cornerA = hit;
      if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point = hit;
      m_step = 1;
      // Immediate feedback before the next move arrives.
      if (ctx.SetPreviewEdges)
      {
        ctx.SetPreviewEdges(make_point_marker(m_cornerA));
      }
      if (ctx.RequestRedraw) ctx.RequestRedraw();
      if (ctx.ReportStatus) ctx.ReportStatus(prompt());
      return true;
    }

    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    base_bounds(m_cornerA, hit, minx, maxx, minz, maxz);
    if (std::abs(maxx - minx) < 1e-4 || std::abs(maxz - minz) < 1e-4)
    {
      BREP_WARN("CreateBoxTool base too small dx={:.6f} dz={:.6f}",
                maxx - minx, maxz - minz);
      if (ctx.ReportStatus)
      {
        ctx.ReportStatus(QStringLiteral("底面尺寸过小，请重新指定对角点"));
      }
      return true;
    }
    if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point = hit;
    m_cornerB = hit;
    m_step = 2;
    AccuSnap::clear_feedback(ctx);
    update_preview(ctx, x, y);
    if (ctx.ReportStatus) ctx.ReportStatus(prompt());
    return true;
  }

  double height = 0.0;
  if (!pick_height(ctx, x, y, height))
  {
    BREP_WARN("CreateBoxTool pick height failed at screen=({:.1f},{:.1f})", x,
              y);
    if (ctx.ReportStatus)
    {
      ctx.ReportStatus(QStringLiteral("无法拾取高度，请调整视角后再试"));
    }
    return true;
  }
  BREP_INFO("CreateBoxTool picked height={:.4f}", height);
  commit_box(ctx, height);
  return true;
}

void CreateBoxTool::on_mouse_move(CommandContext& ctx, float x, float y)
{
  if (m_step == 0)
{
    Point3d hover;
    (void)pick_ground(ctx, x, y, hover);
  } else if (m_step == 1 || m_step == 2)
  {
    update_preview(ctx, x, y);
  }
}

void CreateBoxTool::on_cancel(CommandContext& ctx)
{
  clear_preview(ctx);
  m_finished = true;
  m_result = CommandResult::Cancelled(QStringLiteral("已取消创建立方体"));
  if (ctx.ReportStatus) ctx.ReportStatus(m_result.Message);
  BREP_INFO("CreateBoxTool cancelled at step={}", m_step);
}

}  // namespace brep::viewer::commands
