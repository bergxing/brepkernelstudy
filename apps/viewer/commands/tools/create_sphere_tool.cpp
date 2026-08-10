#include "commands/tools/create_sphere_tool.hpp"

#include "adapter/scene_adapter.hpp"
#include "commands/document_history.hpp"
#include "commands/snap/accusnap.hpp"

#include "api/core.hpp"
#include "api/mesh.hpp"
#include "api/modeling.hpp"

#include <QCoreApplication>
#include <QMouseEvent>

#include <cmath>
#include <numbers>

namespace brep::viewer::commands {
namespace {

QString tr_sphere(const char* source) {
  return QCoreApplication::translate("CreateSphereTool", source);
}

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

EdgeMesh make_sphere_wire(const Point3d& c, double r, int seg = 32) {
  EdgeMesh mesh;
  auto ring = [&](char axis) {
    for (int i = 0; i < seg; ++i) {
      const double t0 = 2.0 * std::numbers::pi * static_cast<double>(i) / seg;
      const double t1 = 2.0 * std::numbers::pi * static_cast<double>(i + 1) / seg;
      Point3d a = c;
      Point3d b = c;
      if (axis == 'y') {
        a = Point3d{c.x() + r * std::cos(t0), c.y(), c.z() + r * std::sin(t0)};
        b = Point3d{c.x() + r * std::cos(t1), c.y(), c.z() + r * std::sin(t1)};
      } else if (axis == 'x') {
        a = Point3d{c.x(), c.y() + r * std::cos(t0), c.z() + r * std::sin(t0)};
        b = Point3d{c.x(), c.y() + r * std::cos(t1), c.z() + r * std::sin(t1)};
      } else {
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
TriangleMesh make_sphere_solid(const Point3d& center, double r) {
  if (!(r > 0.0)) return {};
  Model model;
  Body* body = make_sphere(model, SphereSpec{.center = center, .radius = r,
                                             .name = "preview"});
  if (!body) return {};
  return tessellate_body(*body, TessellationOptions::for_radius(r));
}

}  // namespace

QString CreateSphereTool::prompt() const {
  if (step_ == 0) {
    return tr_sphere("Create sphere: pick center on a surface or ground (ESC cancel)");
  }
  return tr_sphere("Create sphere: pick radius point (ESC cancel)");
}

void CreateSphereTool::on_start(CommandContext& ctx) {
  step_ = 0;
  finished_ = false;
  result_ = CommandResult::cancelled();
  if (ctx.snap_session) ctx.snap_session->last_point.reset();
  clear_preview(ctx);
  if (ctx.report_status) ctx.report_status(prompt());
}

bool CreateSphereTool::pick_point(CommandContext& ctx, float x, float y,
                                  Point3d& hit) const {
  const PickResult result = AccuSnap::resolve(ctx, x, y);
  if (result.kind == SnapKind::None) return false;
  hit = result.point;
  return true;
}

void CreateSphereTool::clear_preview(CommandContext& ctx) {
  if (ctx.clear_preview) ctx.clear_preview();
}

void CreateSphereTool::update_preview(CommandContext& ctx, float x, float y) {
  if (step_ != 1) return;
  Point3d hit;
  if (!pick_point(ctx, x, y, hit)) return;
  const double radius = (hit - center_).norm();
  if (radius < 1e-4) return;

  EdgeMesh wire = make_sphere_wire(center_, radius);
  const EdgeMesh marker = make_point_marker(center_);
  wire.positions.insert(wire.positions.end(), marker.positions.begin(),
                        marker.positions.end());
  TriangleMesh solid = make_sphere_solid(center_, radius);
  if (ctx.set_preview) ctx.set_preview(std::move(wire), std::move(solid));
  else if (ctx.set_preview_edges) ctx.set_preview_edges(std::move(wire));
  if (ctx.request_redraw) ctx.request_redraw();
}

void CreateSphereTool::commit_sphere(CommandContext& ctx, double radius) {
  clear_preview(ctx);
  if (!ctx.world || !ctx.world->document()) {
    result_ = CommandResult::failed(tr_sphere("No active document"));
    finished_ = true;
    return;
  }

  adapter::SceneAdapter scene(ctx.world->document());
  Part* part = scene.main_part();
  if (!part) {
    result_ = CommandResult::failed(tr_sphere("No main part"));
    finished_ = true;
    return;
  }

  SphereSpec spec;
  spec.center = center_;
  spec.radius = radius;
  spec.name = "sphere";

  Body* body = scene.add_sphere(spec);
  if (!body) {
    result_ = CommandResult::failed(tr_sphere("Failed to create sphere"));
    finished_ = true;
    return;
  }

  const feat::FeatureId fid =
      scene.feature_id_for(Guid{}, body->guid).value_or(feat::FeatureId{});
  if (!fid.is_nil()) {
    scene.record_append_sphere(fid, spec);
  }
  const Guid feature_guid = fid.guid;

  Material material = ctx.wood_albedo_path.empty()
                          ? Material{}
                          : make_wood_material(ctx.wood_albedo_path);
  auto mesh = scene.mesh_for_body(body->guid);
  ctx.world->create_body_renderable(body->name, body->guid,
                                    std::move(mesh.faces),
                                    std::move(mesh.edges), material, Point3d{},
                                    feature_guid);
  if (ctx.request_redraw) ctx.request_redraw();
  if (ctx.session) ctx.session->mark_dirty();

  const Guid guid = body->guid;
  const std::string wood = ctx.wood_albedo_path;
  ecs::World* world = ctx.world;
  Part* part_ptr = part;

  if (ctx.history) {
    ctx.history->push(DocumentHistory::Entry{
        .label = tr_sphere("Create sphere"),
        .undo =
            [world, part_ptr, wood, session = ctx.session,
             redraw = ctx.request_redraw, refresh = ctx.refresh_ui] {
              if (!world || !part_ptr) return;
              adapter::SceneAdapter scene_u(world->document());
              scene_u.undo_feature();
              Material material =
                  wood.empty() ? Material{} : make_wood_material(wood);
              world->sync_part_bodies(*part_ptr, std::move(material));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
        .redo =
            [world, part_ptr, wood, session = ctx.session,
             redraw = ctx.request_redraw, refresh = ctx.refresh_ui] {
              if (!world || !part_ptr) return;
              adapter::SceneAdapter scene_r(world->document());
              scene_r.redo_feature();
              Material material =
                  wood.empty() ? Material{} : make_wood_material(wood);
              world->sync_part_bodies(*part_ptr, std::move(material));
              if (session) session->mark_dirty();
              if (redraw) redraw();
              if (refresh) refresh();
            },
    });
  }

  result_ = CommandResult::ok(
      tr_sphere("Created sphere %1")
          .arg(QString::fromStdString(guid.to_string())));
  finished_ = true;
  if (ctx.refresh_ui) ctx.refresh_ui();
}

bool CreateSphereTool::on_mouse_press(CommandContext& ctx, float x, float y,
                                      int button) {
  if (button != Qt::LeftButton) return false;

  Point3d hit;
  if (!pick_point(ctx, x, y, hit)) {
    if (ctx.report_status) {
      ctx.report_status(
          tr_sphere("Missed surface/ground — try another angle"));
    }
    return true;
  }
  if (step_ == 0) {
    center_ = hit;
    if (ctx.snap_session) ctx.snap_session->last_point = hit;
    step_ = 1;
    if (ctx.set_preview_edges) {
      ctx.set_preview_edges(make_point_marker(center_));
    }
    if (ctx.request_redraw) ctx.request_redraw();
    if (ctx.report_status) ctx.report_status(prompt());
    return true;
  }

  const double radius = (hit - center_).norm();
  if (radius < 1e-4) {
    if (ctx.report_status) {
      ctx.report_status(tr_sphere("Radius too small — pick farther"));
    }
    return true;
  }
  if (ctx.snap_session) ctx.snap_session->last_point = hit;
  commit_sphere(ctx, radius);
  return true;
}

void CreateSphereTool::on_mouse_move(CommandContext& ctx, float x, float y) {
  if (step_ == 0) {
    Point3d hover;
    (void)pick_point(ctx, x, y, hover);
  } else if (step_ == 1) {
    update_preview(ctx, x, y);
  }
}

void CreateSphereTool::on_cancel(CommandContext& ctx) {
  clear_preview(ctx);
  finished_ = true;
  result_ = CommandResult::cancelled(tr_sphere("Cancelled create sphere"));
  if (ctx.report_status) ctx.report_status(result_.message);
}

}  // namespace brep::viewer::commands
