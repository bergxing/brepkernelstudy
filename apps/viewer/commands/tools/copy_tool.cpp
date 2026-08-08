#include "commands/tools/copy_tool.hpp"

#include "commands/document_history.hpp"
#include "commands/picking.hpp"
#include "ecs/components.hpp"
#include "ecs/systems.hpp"

#include "brep/brep.hpp"
#include "brep/feat/box_feature.hpp"
#include "brep/log.hpp"

#include <QMouseEvent>

#include <cmath>

namespace brep::viewer::commands {
namespace {

void push_seg(EdgeMesh& mesh, const Point3d& a, const Point3d& b) {
  mesh.positions.push_back(a);
  mesh.positions.push_back(b);
}

EdgeMesh make_point_marker(const Point3d& p, double s = 0.12) {
  EdgeMesh mesh;
  push_seg(mesh, Point3d{p.x() - s, p.y(), p.z()},
           Point3d{p.x() + s, p.y(), p.z()});
  push_seg(mesh, Point3d{p.x(), p.y(), p.z() - s},
           Point3d{p.x(), p.y(), p.z() + s});
  push_seg(mesh, Point3d{p.x(), p.y() - s, p.z()},
           Point3d{p.x(), p.y() + s, p.z()});
  return mesh;
}

EdgeMesh make_box_wire(const BoxSpec& spec) {
  EdgeMesh mesh;
  const Point3d& mn = spec.min;
  const Point3d& mx = spec.max;
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

BoxSpec translated(const BoxSpec& src, const Vector3d& offset) {
  BoxSpec out = src;
  out.min += offset;
  out.max += offset;
  if (out.name.empty()) out.name = "box";
  out.name += "_copy";
  return out;
}

const feat::BoxFeature* resolve_box_feature(brep::Part& part,
                                            entt::registry& registry,
                                            entt::entity entity) {
  if (entity == entt::null || !registry.valid(entity)) return nullptr;

  if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity)) {
    if (auto* f =
            part.features().find(feat::FeatureId{fref->feature_guid})) {
      if (f->type_name() == "Box") {
        return static_cast<const feat::BoxFeature*>(f);
      }
    }
  }
  if (const auto* body = registry.try_get<ecs::BodyRef>(entity)) {
    if (auto* f = part.features().find_by_body(body->guid)) {
      if (f->type_name() == "Box") {
        return static_cast<const feat::BoxFeature*>(f);
      }
    }
  }
  return nullptr;
}

std::vector<BoxSpec> collect_selected_box_specs(CommandContext& ctx) {
  std::vector<BoxSpec> specs;
  if (!ctx.world || !ctx.world->document()) return specs;
  brep::Part* part = ctx.world->document()->main_part();
  if (!part) return specs;

  auto& registry = ctx.world->registry();
  auto view = registry.view<ecs::SelectedTag>();
  for (auto entity : view) {
    const feat::BoxFeature* box =
        resolve_box_feature(*part, registry, entity);
    if (!box) continue;
    specs.push_back(box->to_spec(part->parameters()));
  }
  return specs;
}

}  // namespace

QString CopyTool::prompt() const {
  if (step_ == 0) {
    return QStringLiteral("复制: 选择基点 (ESC 取消)");
  }
  return QStringLiteral("复制: 选择放置点 (ESC 取消)");
}

void CopyTool::clear_preview(CommandContext& ctx) {
  if (ctx.clear_preview) ctx.clear_preview();
}

void CopyTool::on_start(CommandContext& ctx) {
  step_ = 0;
  finished_ = false;
  result_ = CommandResult::cancelled();
  sources_ = collect_selected_box_specs(ctx);
  clear_preview(ctx);

  if (sources_.empty()) {
    result_ = CommandResult::failed(
        QStringLiteral("请先选中至少一个立方体再复制"));
    finished_ = true;
    if (ctx.report_status) ctx.report_status(result_.message);
    BREP_WARN("CopyTool start aborted: no selected boxes");
    return;
  }

  if (ctx.report_status) ctx.report_status(prompt());
  BREP_INFO("CopyTool start with {} source box(es)", sources_.size());
}

bool CopyTool::pick_ground(CommandContext& ctx, float x, float y,
                           Point3d& hit) const {
  Camera* cam = ctx.view_camera
                    ? ctx.view_camera
                    : (ctx.world ? ctx.world->main_camera() : nullptr);
  if (!cam) return false;
  Point3d origin;
  Vector3d dir;
  if (!screen_to_ray(*cam, ctx.viewport_w, ctx.viewport_h, x, y, origin, dir)) {
    return false;
  }
  return intersect_plane_y(origin, dir, 0.0, hit);
}

void CopyTool::update_preview(CommandContext& ctx, float x, float y) {
  if (!ctx.set_preview_edges) return;
  Point3d place;
  if (!pick_ground(ctx, x, y, place)) return;

  EdgeMesh mesh = make_point_marker(base_);
  push_seg(mesh, base_, place);
  {
    EdgeMesh tip = make_point_marker(place);
    mesh.positions.insert(mesh.positions.end(), tip.positions.begin(),
                          tip.positions.end());
  }

  const Vector3d offset{place.x() - base_.x(), place.y() - base_.y(),
                        place.z() - base_.z()};
  for (const auto& src : sources_) {
    EdgeMesh box = make_box_wire(translated(src, offset));
    mesh.positions.insert(mesh.positions.end(), box.positions.begin(),
                          box.positions.end());
  }
  ctx.set_preview_edges(std::move(mesh));
  if (ctx.request_redraw) ctx.request_redraw();
}

void CopyTool::commit_copies(CommandContext& ctx, const Point3d& place) {
  clear_preview(ctx);
  if (!ctx.world || !ctx.world->document()) {
    result_ = CommandResult::failed(QStringLiteral("无文档"));
    finished_ = true;
    return;
  }
  Part* part = ctx.world->document()->main_part();
  if (!part) {
    result_ = CommandResult::failed(QStringLiteral("无零件"));
    finished_ = true;
    return;
  }

  const Vector3d offset{place.x() - base_.x(), place.y() - base_.y(),
                        place.z() - base_.z()};
  if (offset.norm() < 1e-6) {
    result_ = CommandResult::failed(QStringLiteral("放置点与基点重合，未复制"));
    finished_ = true;
    return;
  }

  Material material = ctx.wood_albedo_path.empty()
                          ? Material{}
                          : make_wood_material(ctx.wood_albedo_path);

  int created = 0;
  for (const auto& src : sources_) {
    BoxSpec spec = translated(src, offset);
    Body* body = part->add_box(spec);
    if (!body) continue;

    Guid feature_guid{};
    if (const auto* feature = part->features().find_by_body(body->guid)) {
      feature_guid = feature->id().guid;
      feat::FeatureTransaction tx;
      tx.kind = feat::TxKind::AppendFeature;
      tx.feature = feature->id();
      tx.feature_type = "Box";
      tx.box_spec = spec;
      part->feature_history().record(std::move(tx));
    }

    ctx.world->create_body_renderable(body->name, body->guid,
                                      tessellate_body(*body),
                                      extract_edges(*body), material, Point3d{},
                                      feature_guid);
    ++created;
  }

  if (created == 0) {
    result_ = CommandResult::failed(QStringLiteral("复制失败"));
    finished_ = true;
    return;
  }

  if (ctx.request_redraw) ctx.request_redraw();
  if (ctx.session) ctx.session->mark_dirty();

  const std::string wood = ctx.wood_albedo_path;
  ecs::World* world = ctx.world;
  Part* part_ptr = part;
  const int undo_steps = created;

  if (ctx.history) {
    ctx.history->push(DocumentHistory::Entry{
        .label = QStringLiteral("复制 %1 个对象").arg(created),
        .undo =
            [world, part_ptr, wood, undo_steps, session = ctx.session,
             redraw = ctx.request_redraw, refresh = ctx.refresh_ui] {
              if (!world || !part_ptr) return;
              for (int i = 0; i < undo_steps; ++i) {
                part_ptr->feature_history().undo(*part_ptr);
              }
              Material mat =
                  wood.empty() ? Material{} : make_wood_material(wood);
              world->sync_part_bodies(*part_ptr, std::move(mat));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
        .redo =
            [world, part_ptr, wood, undo_steps, session = ctx.session,
             redraw = ctx.request_redraw, refresh = ctx.refresh_ui] {
              if (!world || !part_ptr) return;
              for (int i = 0; i < undo_steps; ++i) {
                part_ptr->feature_history().redo(*part_ptr);
              }
              Material mat =
                  wood.empty() ? Material{} : make_wood_material(wood);
              world->sync_part_bodies(*part_ptr, std::move(mat));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
    });
  }

  result_ = CommandResult::ok(QStringLiteral("已复制 %1 个对象").arg(created));
  finished_ = true;
  if (ctx.refresh_ui) ctx.refresh_ui();
  BREP_INFO("CopyTool committed {} copies offset=({:.4f},{:.4f},{:.4f})",
            created, offset.x(), offset.y(), offset.z());
}

bool CopyTool::on_mouse_press(CommandContext& ctx, float x, float y,
                              int button) {
  if (button != Qt::LeftButton) return false;

  Point3d hit;
  if (!pick_ground(ctx, x, y, hit)) {
    if (ctx.report_status) {
      ctx.report_status(QStringLiteral("未点到地面 (y=0)，请换个角度再试"));
    }
    return true;
  }

  if (step_ == 0) {
    base_ = hit;
    step_ = 1;
    if (ctx.set_preview_edges) {
      ctx.set_preview_edges(make_point_marker(base_));
    }
    if (ctx.request_redraw) ctx.request_redraw();
    if (ctx.report_status) ctx.report_status(prompt());
    return true;
  }

  commit_copies(ctx, hit);
  return true;
}

void CopyTool::on_mouse_move(CommandContext& ctx, float x, float y) {
  if (step_ == 1) update_preview(ctx, x, y);
}

void CopyTool::on_cancel(CommandContext& ctx) {
  clear_preview(ctx);
  finished_ = true;
  result_ = CommandResult::cancelled(QStringLiteral("已取消复制"));
  if (ctx.report_status) ctx.report_status(result_.message);
}

}  // namespace brep::viewer::commands
