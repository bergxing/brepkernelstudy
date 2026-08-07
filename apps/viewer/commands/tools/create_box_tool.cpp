#include "commands/tools/create_box_tool.hpp"

#include "commands/document_history.hpp"
#include "commands/picking.hpp"

#include "brep/brep.hpp"
#include "brep/log.hpp"

#include <QMouseEvent>

#include <algorithm>
#include <cmath>

namespace brep::viewer::commands {
namespace {

brep::Body* find_body(brep::Document& doc, const brep::Guid& guid) {
  for (const auto& part : doc.parts()) {
    for (const auto& body : part->model().bodies()) {
      if (body && body->guid == guid) return body.get();
    }
  }
  return nullptr;
}

void show_body(CommandContext& ctx, brep::Body& body) {
  using namespace brep;
  Material material = ctx.wood_albedo_path.empty()
                          ? Material{}
                          : make_wood_material(ctx.wood_albedo_path);
  ctx.world->create_body_renderable(body.name, body.guid, tessellate_body(body),
                                    extract_edges(body), std::move(material),
                                    Point3d{0, 0, 0});
  if (ctx.request_redraw) ctx.request_redraw();
}

void push_seg(EdgeMesh& mesh, const Point3d& a, const Point3d& b) {
  mesh.positions.push_back(a);
  mesh.positions.push_back(b);
}

EdgeMesh make_rect_wire(double minx, double minz, double maxx, double maxz,
                        double y) {
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
                       double maxy, double maxz) {
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

void base_bounds(const Point3d& a, const Point3d& b, double& minx, double& maxx,
                 double& minz, double& maxz) {
  minx = std::min(a.x(), b.x());
  maxx = std::max(a.x(), b.x());
  minz = std::min(a.z(), b.z());
  maxz = std::max(a.z(), b.z());
}

}  // namespace

QString CreateBoxTool::prompt() const {
  switch (step_) {
    case 0:
      return QStringLiteral("创建立方体: 拾取底面第一个角点 (ESC 取消)");
    case 1:
      return QStringLiteral("创建立方体: 拾取底面对角点 (ESC 取消)");
    default:
      return QStringLiteral("创建立方体: 拾取高度点 (ESC 取消)");
  }
}

void CreateBoxTool::clear_preview(CommandContext& ctx) {
  if (ctx.clear_preview) ctx.clear_preview();
}

void CreateBoxTool::on_start(CommandContext& ctx) {
  step_ = 0;
  finished_ = false;
  result_ = CommandResult::cancelled();
  clear_preview(ctx);
  if (ctx.report_status) ctx.report_status(prompt());
}

bool CreateBoxTool::pick_ground(CommandContext& ctx, float x, float y,
                                Point3d& hit) const {
  Camera* cam = ctx.world ? ctx.world->main_camera() : nullptr;
  if (!cam) return false;
  Point3d origin;
  Vector3d dir;
  if (!screen_to_ray(*cam, ctx.viewport_w, ctx.viewport_h, x, y, origin, dir)) {
    return false;
  }
  return intersect_plane_y(origin, dir, 0.0, hit);
}

bool CreateBoxTool::pick_height(CommandContext& ctx, float x, float y,
                                double& height) const {
  Camera* cam = ctx.world ? ctx.world->main_camera() : nullptr;
  if (!cam) return false;

  Point3d origin;
  Vector3d dir;
  if (!screen_to_ray(*cam, ctx.viewport_w, ctx.viewport_h, x, y, origin, dir)) {
    return false;
  }

  double minx = 0, maxx = 0, minz = 0, maxz = 0;
  base_bounds(corner_a_, corner_b_, minx, maxx, minz, maxz);
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

void CreateBoxTool::update_preview(CommandContext& ctx, float x, float y) {
  if (!ctx.set_preview_edges) return;

  if (step_ == 1) {
    Point3d hit;
    if (!pick_ground(ctx, x, y, hit)) return;
    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    base_bounds(corner_a_, hit, minx, maxx, minz, maxz);
    if (std::abs(maxx - minx) < 1e-6 && std::abs(maxz - minz) < 1e-6) return;
    ctx.set_preview_edges(make_rect_wire(minx, minz, maxx, maxz, 0.0));
    if (ctx.request_redraw) ctx.request_redraw();
    return;
  }

  if (step_ == 2) {
    double height = 0.0;
    if (!pick_height(ctx, x, y, height)) return;
    if (std::abs(height) < 1e-4) height = (height < 0.0) ? -1e-3 : 1e-3;
    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    base_bounds(corner_a_, corner_b_, minx, maxx, minz, maxz);
    const double miny = std::min(0.0, height);
    const double maxy = std::max(0.0, height);
    ctx.set_preview_edges(make_box_wire(minx, miny, minz, maxx, maxy, maxz));
    if (ctx.request_redraw) ctx.request_redraw();
  }
}

void CreateBoxTool::commit_box(CommandContext& ctx, double height) {
  using namespace brep;
  clear_preview(ctx);

  Part* part = ctx.world->document()->main_part();
  if (!part) {
    result_ = CommandResult::failed(QStringLiteral("当前没有 Part"));
    finished_ = true;
    return;
  }

  double minx = 0, maxx = 0, minz = 0, maxz = 0;
  base_bounds(corner_a_, corner_b_, minx, maxx, minz, maxz);
  if (std::abs(maxx - minx) < 1e-4 || std::abs(maxz - minz) < 1e-4 ||
      std::abs(height) < 1e-4) {
    result_ = CommandResult::failed(QStringLiteral("盒子尺寸过小，请重新拾取"));
    finished_ = true;
    return;
  }

  const double miny = std::min(0.0, height);
  const double maxy = std::max(0.0, height);

  Body* body = part->add_box(BoxSpec{
      .min = Point3d{minx, miny, minz},
      .max = Point3d{maxx, maxy, maxz},
      .name = "box",
  });
  show_body(ctx, *body);
  if (ctx.session) ctx.session->mark_dirty();

  const Guid guid = body->guid;
  const std::string wood = ctx.wood_albedo_path;
  ecs::World* world = ctx.world;

  if (ctx.history) {
    ctx.history->push(DocumentHistory::Entry{
        .label = QStringLiteral("创建盒子"),
        .undo =
            [world, guid, session = ctx.session, redraw = ctx.request_redraw,
             refresh = ctx.refresh_ui] {
              if (!world) return;
              world->destroy_body_renderable(guid);
              if (auto* doc = world->document()) {
                doc->registry().remove(guid);
              }
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
        .redo =
            [world, guid, wood, session = ctx.session,
             redraw = ctx.request_redraw, refresh = ctx.refresh_ui] {
              if (!world || !world->document()) return;
              Body* body = find_body(*world->document(), guid);
              if (!body) return;
              world->document()->registry().add(*body);
              CommandContext tmp;
              tmp.world = world;
              tmp.wood_albedo_path = wood;
              tmp.request_redraw = redraw;
              show_body(tmp, *body);
              if (session) session->mark_dirty();
              if (refresh) refresh();
            },
    });
  }

  result_ = CommandResult::ok(
      QStringLiteral("已创建盒子 %1")
          .arg(QString::fromStdString(guid.to_string())));
  finished_ = true;
  if (ctx.refresh_ui) ctx.refresh_ui();
  BREP_INFO("CreateBoxTool committed guid={}", guid.to_string());
}

bool CreateBoxTool::on_mouse_press(CommandContext& ctx, float x, float y,
                                   int button) {
  if (button != Qt::LeftButton) return false;

  if (step_ == 0 || step_ == 1) {
    Point3d hit;
    if (!pick_ground(ctx, x, y, hit)) {
      if (ctx.report_status) {
        ctx.report_status(QStringLiteral("未点到地面 (y=0)，请换个角度再试"));
      }
      return true;
    }

    if (step_ == 0) {
      corner_a_ = hit;
      step_ = 1;
      if (ctx.report_status) ctx.report_status(prompt());
      return true;
    }

    double minx = 0, maxx = 0, minz = 0, maxz = 0;
    base_bounds(corner_a_, hit, minx, maxx, minz, maxz);
    if (std::abs(maxx - minx) < 1e-4 || std::abs(maxz - minz) < 1e-4) {
      if (ctx.report_status) {
        ctx.report_status(QStringLiteral("底面尺寸过小，请重新指定对角点"));
      }
      return true;
    }
    corner_b_ = hit;
    step_ = 2;
    update_preview(ctx, x, y);
    if (ctx.report_status) ctx.report_status(prompt());
    return true;
  }

  double height = 0.0;
  if (!pick_height(ctx, x, y, height)) {
    if (ctx.report_status) {
      ctx.report_status(QStringLiteral("无法拾取高度，请调整视角后再试"));
    }
    return true;
  }
  commit_box(ctx, height);
  return true;
}

void CreateBoxTool::on_mouse_move(CommandContext& ctx, float x, float y) {
  if (step_ == 1 || step_ == 2) {
    update_preview(ctx, x, y);
  }
}

void CreateBoxTool::on_cancel(CommandContext& ctx) {
  clear_preview(ctx);
  finished_ = true;
  result_ = CommandResult::cancelled(QStringLiteral("已取消创建立方体"));
  if (ctx.report_status) ctx.report_status(result_.message);
}

}  // namespace brep::viewer::commands
