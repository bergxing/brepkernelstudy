#include "commands/tools/CopyTool.h"

#include "adapter/SceneAdapter.h"
#include "commands/DocumentHistory.h"
#include "commands/snap/Accusnap.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include <QMouseEvent>
#include <Qnamespace.h>

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

EdgeMesh make_box_wire(const BoxSpec& spec)
{
  EdgeMesh mesh;
  const Point3d& mn = spec.Min;
  const Point3d& mx = spec.Max;
  const Point3d p000{mn.x(), mn.y(), mn.z()};
  const Point3d p100{mx.x(), mn.y(), mn.z()};
  const Point3d p110{mx.x(), mn.y(), mx.z()};
  const Point3d p010{mn.x(), mn.y(), mx.z()};
  const Point3d p001{mn.x(), mx.y(), mn.z()};
  const Point3d p101{mx.x(), mx.y(), mn.z()};
  const Point3d p111{mx.x(), mx.y(), mx.z()};
  const Point3d p011{mn.x(), mx.y(), mx.z()};
  push_seg(mesh, p000, p100);
  push_seg(mesh, p100, p110);
  push_seg(mesh, p110, p010);
  push_seg(mesh, p010, p000);
  push_seg(mesh, p001, p101);
  push_seg(mesh, p101, p111);
  push_seg(mesh, p111, p011);
  push_seg(mesh, p011, p001);
  push_seg(mesh, p000, p001);
  push_seg(mesh, p100, p101);
  push_seg(mesh, p110, p111);
  push_seg(mesh, p010, p011);
  return mesh;
}

BoxSpec translated(const BoxSpec& src, const Vector3d& offset)
{
  BoxSpec out = src;
  out.Min += offset;
  out.Max += offset;
  if (out.Name.empty()) out.Name = "box";
  out.Name += "_copy";
  return out;
}

std::vector<BoxSpec> collect_selected_box_specs(CommandContext& ctx)
{
  std::vector<BoxSpec> specs;
  if (!ctx.World || !ctx.World->document()) return specs;
  adapter::SceneAdapter scene(ctx.World->document());
  if (!scene.main_part()) return specs;

  auto& registry = ctx.World->registry();
  auto view = registry.view<ecs::SelectedTag>();
  for (auto entity : view)
  {
    Guid feature_guid{};
    Guid body_guid{};
    if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity))
    {
      feature_guid = fref->feature_guid;
    }
    if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
    {
      body_guid = body->guid;
    }
    if (auto spec = scene.box_spec_for(feature_guid, body_guid))
    {
      specs.push_back(*spec);
    }
  }
  return specs;
}

}  // namespace

QString CopyTool::prompt() const
{
  switch (m_step)
{
    case 0:
      return QStringLiteral(
          "??: ??/??/Ctrl+??????????? (ESC ??)");
    case 1:
      return QStringLiteral("??: ???? (ESC ??)");
    default:
      return QStringLiteral("??: ????? (ESC ??)");
  }
}

void CopyTool::clear_preview(CommandContext& ctx)
{
  if (ctx.ClearPreview) ctx.ClearPreview();
}

void CopyTool::on_start(CommandContext& ctx)
{
  m_step = 0;
  m_finished = false;
  m_result = CommandResult::Cancelled();
  m_sources.clear();
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point.reset();
  clear_preview(ctx);

  // Path A: objects already selected ? skip select step, go to base point.
  m_sources = collect_selected_box_specs(ctx);
  if (!m_sources.empty())
  {
    m_step = 1;
    if (ctx.ReportStatus) ctx.ReportStatus(prompt());
    BREP_INFO("CopyTool start with {} pre-selected box(es)", m_sources.size());
    return;
  }

  // Path B: no selection ? pick objects, Space to confirm.
  if (ctx.ReportStatus) ctx.ReportStatus(prompt());
  BREP_INFO("CopyTool start (select objects, then Space)");
}

bool CopyTool::confirm_selection(CommandContext& ctx)
{
  m_sources = collect_selected_box_specs(ctx);
  if (m_sources.empty())
  {
    if (ctx.ReportStatus)
  {
      ctx.ReportStatus(
          QStringLiteral("??????????????????"));
    }
    return false;
  }
  m_step = 1;
  clear_preview(ctx);
  if (ctx.ReportStatus) ctx.ReportStatus(prompt());
  BREP_INFO("CopyTool selection confirmed: {} box(es)", m_sources.size());
  return true;
}

bool CopyTool::pick_ground(CommandContext& ctx, float x, float y,
                           Point3d& hit) const
{
  const PickResult result = AccuSnap::resolve(ctx, x, y);
  if (result.kind == SnapKind::None) return false;
  hit = result.point;
  return true;
}

void CopyTool::update_preview(CommandContext& ctx, float x, float y)
{
  if (!ctx.SetPreviewEdges) return;
  Point3d place;
  if (!pick_ground(ctx, x, y, place)) return;

  EdgeMesh mesh = make_point_marker(m_base);
  push_seg(mesh, m_base, place);
{
    EdgeMesh tip = make_point_marker(place);
    mesh.Positions.insert(mesh.Positions.end(), tip.Positions.begin(),
                          tip.Positions.end());
  }

  const Vector3d offset{place.x() - m_base.x(), place.y() - m_base.y(),
                        place.z() - m_base.z()};
  for (const auto& src : m_sources)
  {
    EdgeMesh box = make_box_wire(translated(src, offset));
    mesh.Positions.insert(mesh.Positions.end(), box.Positions.begin(),
                          box.Positions.end());
  }
  ctx.SetPreviewEdges(std::move(mesh));
  if (ctx.RequestRedraw) ctx.RequestRedraw();
}

void CopyTool::commit_copies(CommandContext& ctx, const Point3d& place)
{
  clear_preview(ctx);
  if (!ctx.World || !ctx.World->document())
  {
    m_result = CommandResult::Failed(QStringLiteral("???"));
    m_finished = true;
    return;
  }
  adapter::SceneAdapter scene(ctx.World->document());
  Part* part = scene.main_part();
  if (!part)
  {
    m_result = CommandResult::Failed(QStringLiteral("???"));
    m_finished = true;
    return;
  }

  const Vector3d offset{place.x() - m_base.x(), place.y() - m_base.y(),
                        place.z() - m_base.z()};
  if (offset.norm() < 1e-6)
  {
    m_result = CommandResult::Failed(QStringLiteral("????????????"));
    m_finished = true;
    return;
  }

  Material material = ctx.WoodAlbedoPath.empty()
                          ? Material{}
                          : MakeWoodMaterial(ctx.WoodAlbedoPath);

  int created = 0;
  for (const auto& src : m_sources)
  {
    BoxSpec spec = translated(src, offset);
    Body* body = scene.add_box(spec);
    if (!body) continue;

    Guid feature_guid{};
    if (auto obj = scene.object_for_body(body->Guid))
    {
      feature_guid = obj->feature_guid;
      scene.record_append_feature(feat::FeatureId{feature_guid}, spec);
    }

    auto mesh = scene.mesh_for_body(body->Guid);
    ctx.World->create_body_renderable(body->Name, body->Guid,
                                      std::move(mesh.faces),
                                      std::move(mesh.edges), material, Point3d{},
                                      feature_guid);
    ++created;
  }

  if (created == 0)
  {
    m_result = CommandResult::Failed(QStringLiteral("????"));
    m_finished = true;
    return;
  }

  if (ctx.RequestRedraw) ctx.RequestRedraw();
  if (ctx.Session) ctx.Session->mark_dirty();

  const std::string wood = ctx.WoodAlbedoPath;
  ecs::World* world = ctx.World;
  Part* part_ptr = part;
  const int undo_steps = created;

  if (ctx.History)
  {
    ctx.History->push(DocumentHistory::Entry{
        .label = QStringLiteral("?? %1 ???").arg(created),
        .undo =
            [world, part_ptr, wood, undo_steps, session = ctx.Session,
             redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
              if (!world || !part_ptr) return;
              adapter::SceneAdapter scene_u(world->document());
              scene_u.undo_feature(undo_steps);
              Material mat =
                  wood.empty() ? Material{} : MakeWoodMaterial(wood);
              world->sync_part_bodies(*part_ptr, std::move(mat));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
        .redo =
            [world, part_ptr, wood, undo_steps, session = ctx.Session,
             redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
              if (!world || !part_ptr) return;
              adapter::SceneAdapter scene_r(world->document());
              scene_r.redo_feature(undo_steps);
              Material mat =
                  wood.empty() ? Material{} : MakeWoodMaterial(wood);
              world->sync_part_bodies(*part_ptr, std::move(mat));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
    });
  }

  m_result = CommandResult::Ok(QStringLiteral("??? %1 ???").arg(created));
  m_finished = true;
  if (ctx.RefreshUi) ctx.RefreshUi();
  BREP_INFO("CopyTool committed {} copies offset=({:.4f},{:.4f},{:.4f})",
            created, offset.x(), offset.y(), offset.z());
}

bool CopyTool::on_mouse_press(CommandContext& ctx, float x, float y,
                              int button)
{
  // Selection phase: let the viewport own click / box / Ctrl+select.
  if (m_step == 0) return false;
  if (button != Qt::LeftButton) return false;

  Point3d hit;
  if (!pick_ground(ctx, x, y, hit))
  {
    if (ctx.ReportStatus)
  {
      ctx.ReportStatus(QStringLiteral("????? (y=0)????????"));
    }
    return true;
  }
  if (m_step == 1)
  {
    m_base = hit;
    if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point = hit;
    m_step = 2;
    if (ctx.SetPreviewEdges)
    {
      ctx.SetPreviewEdges(make_point_marker(m_base));
    }
    if (ctx.RequestRedraw) ctx.RequestRedraw();
    if (ctx.ReportStatus) ctx.ReportStatus(prompt());
    return true;
  }

  const Vector3d offset{hit.x() - m_base.x(), hit.y() - m_base.y(),
                        hit.z() - m_base.z()};
  if (offset.norm() < 1e-6)
  {
    if (ctx.ReportStatus)
  {
      ctx.ReportStatus(QStringLiteral("????????????"));
    }
    return true;
  }
  if (ctx.SnapSessionRef) ctx.SnapSessionRef->last_point = hit;
  commit_copies(ctx, hit);
  return true;
}

void CopyTool::on_mouse_move(CommandContext& ctx, float x, float y)
{
  if (m_step == 1)
{
    Point3d hover;
    (void)pick_ground(ctx, x, y, hover);
  } else if (m_step == 2)
  {
    update_preview(ctx, x, y);
  }
}

bool CopyTool::on_key_press(CommandContext& ctx, int key)
{
  if (m_step != 0) return false;
  if (key != Qt::Key_Space && key != Qt::Key_Return && key != Qt::Key_Enter)
  {
    return false;
  }
  confirm_selection(ctx);
  return true;
}

void CopyTool::on_cancel(CommandContext& ctx)
{
  clear_preview(ctx);
  m_finished = true;
  m_result = CommandResult::Cancelled(QStringLiteral("?????"));
  if (ctx.ReportStatus) ctx.ReportStatus(m_result.Message);
}

}  // namespace brep::viewer::commands
