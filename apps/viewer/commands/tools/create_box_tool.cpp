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

}  // namespace

QString CreateBoxTool::prompt() const {
  if (step_ == 0) {
    return QStringLiteral("创建立方体: 单击地面指定第一个角点 (ESC 取消)");
  }
  return QStringLiteral("创建立方体: 单击指定对角点 (ESC 取消)");
}

void CreateBoxTool::on_start(CommandContext& ctx) {
  step_ = 0;
  finished_ = false;
  result_ = CommandResult::cancelled();
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

void CreateBoxTool::commit_box(CommandContext& ctx, const Point3d& a,
                               const Point3d& b) {
  using namespace brep;
  Part* part = ctx.world->document()->main_part();
  if (!part) {
    result_ = CommandResult::failed(QStringLiteral("当前没有 Part"));
    finished_ = true;
    return;
  }

  const double minx = std::min(a.x(), b.x());
  const double maxx = std::max(a.x(), b.x());
  const double minz = std::min(a.z(), b.z());
  const double maxz = std::max(a.z(), b.z());
  constexpr double height = 1.0;
  if (std::abs(maxx - minx) < 1e-4 || std::abs(maxz - minz) < 1e-4) {
    result_ = CommandResult::failed(QStringLiteral("盒子尺寸过小，请重新拾取"));
    finished_ = true;
    return;
  }

  Body* body = part->add_box(BoxSpec{
      .min = Point3d{minx, 0.0, minz},
      .max = Point3d{maxx, height, maxz},
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
            [world, guid, wood, session = ctx.session, redraw = ctx.request_redraw,
             refresh = ctx.refresh_ui] {
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

  commit_box(ctx, corner_a_, hit);
  return true;
}

void CreateBoxTool::on_mouse_move(CommandContext& /*ctx*/, float /*x*/,
                                  float /*y*/) {
  // Phase-3 preview mesh can be added later.
}

void CreateBoxTool::on_cancel(CommandContext& ctx) {
  finished_ = true;
  result_ = CommandResult::cancelled(QStringLiteral("已取消创建立方体"));
  if (ctx.report_status) ctx.report_status(result_.message);
}

}  // namespace brep::viewer::commands
