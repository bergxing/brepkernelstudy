#include "brep/bool/sphere_box_boolean.hpp"

#include "brep/geometry.hpp"
#include "brep/log.hpp"
#include "brep/validate.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace brep::boolean {
namespace {

[[nodiscard]] const char* op_name(BooleanOp op) noexcept {
  switch (op) {
    case BooleanOp::Union:
      return "Union";
    case BooleanOp::Subtract:
      return "Subtract";
    case BooleanOp::Intersect:
      return "Intersect";
  }
  return "Unknown";
}

[[nodiscard]] double norm_angle(double t) {
  const double twopi = 2.0 * std::numbers::pi;
  t = std::fmod(t, twopi);
  if (t < 0.0) t += twopi;
  return t;
}

[[nodiscard]] double circle_param_at(const CircleCurve& curve, const Point3d& p) {
  const Point3d p0 = curve.eval(0.0);
  const Point3d p90 = curve.eval(0.5 * std::numbers::pi);
  const Vector3d x_axis = (p0 - curve.center()).normalized();
  const Vector3d y_axis = (p90 - curve.center()).normalized();
  const Vector3d d = p - curve.center();
  return norm_angle(std::atan2(d.dot(y_axis), d.dot(x_axis)));
}

/// Build circular edge along the shorter arc from `a` to `b` (t increasing).
Edge* make_arc_edge(Model& model, CircleCurve* curve, Vertex* a, Vertex* b,
                    double tol, const std::string& name) {
  double t0 = circle_param_at(*curve, a->position());
  double t1 = circle_param_at(*curve, b->position());
  double forward = norm_angle(t1 - t0);
  if (forward > std::numbers::pi) {
    // Prefer shorter arc: reverse endpoints.
    std::swap(a, b);
    std::swap(t0, t1);
    forward = norm_angle(t1 - t0);
  }
  if (t1 < t0) t1 += 2.0 * std::numbers::pi;
  return model.make_edge(curve, a, b, t0, t1, tol, name);
}

[[nodiscard]] Orientation sense_along(Edge* edge, Vertex* from, Vertex* to) {
  if (edge->v0 == from && edge->v1 == to) return Orientation::Forward;
  if (edge->v0 == to && edge->v1 == from) return Orientation::Reversed;
  // Fall back: compare positions.
  const double d0 = (edge->v0->position() - from->position()).squaredNorm();
  const double d1 = (edge->v1->position() - from->position()).squaredNorm();
  return d0 <= d1 ? Orientation::Forward : Orientation::Reversed;
}

Body* build_positive_octant_ball(Model& model, const Point3d& c, double r,
                                 double tol, const std::string& name) {
  const Point3d ax{c.x() + r, c.y(), c.z()};
  const Point3d ay{c.x(), c.y() + r, c.z()};
  const Point3d az{c.x(), c.y(), c.z() + r};

  Vertex* vo = model.make_vertex(model.make_point(c), tol, name + "_o");
  Vertex* vx = model.make_vertex(model.make_point(ax), tol, name + "_ax");
  Vertex* vy = model.make_vertex(model.make_point(ay), tol, name + "_ay");
  Vertex* vz = model.make_vertex(model.make_point(az), tol, name + "_az");

  auto make_line_edge = [&](Vertex* a, Vertex* b, const std::string& en) {
    LineCurve* curve =
        model.make_line(a->position(), b->position(), en + "_crv");
    return model.make_edge(curve, a, b, 0.0, r, tol, en);
  };

  Edge* e_ox = make_line_edge(vo, vx, name + "_eox");
  Edge* e_oy = make_line_edge(vo, vy, name + "_eoy");
  Edge* e_oz = make_line_edge(vo, vz, name + "_eoz");

  CircleCurve* c_xy =
      model.make_circle(c, Vector3d{0, 0, 1}, r, name + "_cxy");
  CircleCurve* c_yz =
      model.make_circle(c, Vector3d{1, 0, 0}, r, name + "_cyz");
  CircleCurve* c_zx =
      model.make_circle(c, Vector3d{0, 1, 0}, r, name + "_czx");

  // Requested directions: Ax→Ay, Ay→Az, Az→Ax (may store reversed internally).
  Edge* e_xy = make_arc_edge(model, c_xy, vx, vy, tol, name + "_exy");
  Edge* e_yz = make_arc_edge(model, c_yz, vy, vz, tol, name + "_eyz");
  Edge* e_zx = make_arc_edge(model, c_zx, vz, vx, tol, name + "_ezx");

  Body* body = model.make_body(BodyType::Solid, name);
  Shell* shell = model.make_shell(true, name + "_shell");
  body->shells.push_back(shell);

  auto add_plane_face = [&](Vector3d u, Vector3d v, Edge* e0, Vertex* a0,
                            Vertex* b0, Edge* e1, Vertex* a1, Vertex* b1,
                            Edge* e2, Vertex* a2, Vertex* b2,
                            const std::string& fname) {
    // Outward = -(u×v) via Reversed face sense when u×v points inward.
    PlaneSurface* surf = model.make_plane(c, u, v, fname);
    Face* face = model.make_face(surf, Orientation::Reversed, fname);
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, fname + "_outer");
    CoEdge* c0 = model.make_coedge(e0, sense_along(e0, a0, b0));
    CoEdge* c1 = model.make_coedge(e1, sense_along(e1, a1, b1));
    CoEdge* c2 = model.make_coedge(e2, sense_along(e2, a2, b2));
    Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
  };

  // z=cz outward -Z: O→Ax→Ay→O ; plane u=X,v=Y → +Z, Reversed → -Z
  add_plane_face({1, 0, 0}, {0, 1, 0}, e_ox, vo, vx, e_xy, vx, vy, e_oy, vy, vo,
                 name + "_fz");
  // x=cx outward -X: O→Ay→Az→O ; u=Y,v=Z → +X, Reversed → -X
  add_plane_face({0, 1, 0}, {0, 0, 1}, e_oy, vo, vy, e_yz, vy, vz, e_oz, vz, vo,
                 name + "_fx");
  // y=cy outward -Y: O→Az→Ax→O ; u=Z,v=X → +Y, Reversed → -Y
  add_plane_face({0, 0, 1}, {1, 0, 0}, e_oz, vo, vz, e_zx, vz, vx, e_ox, vx, vo,
                 name + "_fy");

  // Sphere outward: opposite arc directions → Ax→Az→Ay→Ax
  {
    SphereSurface* surf = model.make_sphere_surface(c, r, name + "_fs");
    Face* face = model.make_face(surf, Orientation::Forward, name + "_fs");
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, name + "_fs_outer");
    CoEdge* c0 = model.make_coedge(e_zx, sense_along(e_zx, vx, vz));  // Ax→Az
    CoEdge* c1 = model.make_coedge(e_yz, sense_along(e_yz, vz, vy));  // Az→Ay
    CoEdge* c2 = model.make_coedge(e_xy, sense_along(e_xy, vy, vx));  // Ay→Ax
    Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
  }

  for (Edge* e : {e_ox, e_oy, e_oz, e_xy, e_yz, e_zx}) {
    if (e->radial.size() != 2) {
      BREP_ERROR("octant ball: edge '{}' radial={}", e->name, e->radial.size());
      return nullptr;
    }
    Model::pair_partners(e->radial[0], e->radial[1]);
  }

  const auto report = validate_body(*body);
  if (!report.ok()) {
    for (const auto& issue : report.issues) {
      BREP_WARN("octant ball validate [{}] {}", issue.where, issue.message);
    }
    return nullptr;
  }
  return body;
}

[[nodiscard]] bool is_contained_axis_octant(const SphereSpec& sphere,
                                            const BoxSpec& box, double eps,
                                            Point3d& origin_out) {
  const Point3d& c = sphere.center;
  const double r = sphere.radius;
  if (std::abs(c.x() - box.min.x()) > eps ||
      std::abs(c.y() - box.min.y()) > eps ||
      std::abs(c.z() - box.min.z()) > eps) {
    return false;
  }
  if (box.max.x() + eps < c.x() + r) return false;
  if (box.max.y() + eps < c.y() + r) return false;
  if (box.max.z() + eps < c.z() + r) return false;
  origin_out = c;
  return true;
}

}  // namespace

BooleanResult evaluate_sphere_box_boolean(BooleanOp op, Model& model,
                                          const SphereSpec& sphere,
                                          const BoxSpec& box, bool sphere_is_a,
                                          const BooleanContext& ctx) {
  BooleanResult result;
  result.mode = BooleanEvalMode::AnalyticPair;
  const double eps = std::max(ctx.fuzzy, 1e-9);
  (void)sphere_is_a;

  if (op != BooleanOp::Intersect) {
    result.diagnostics =
        std::string("boolean ") + op_name(op) +
        ": sphere–box only Intersect (⅛-ball) is implemented";
    BREP_WARN("{}", result.diagnostics);
    return result;
  }

  Point3d origin;
  if (!is_contained_axis_octant(sphere, box, eps, origin)) {
    result.diagnostics =
        "boolean Intersect: sphere–box requires sphere center at box.min and "
        "box containing the +++ octant of the ball";
    BREP_WARN("{}", result.diagnostics);
    return result;
  }

  const std::string name = std::string("bool_sphere_box_") + op_name(op);
  result.body = build_positive_octant_ball(
      model, origin, sphere.radius, std::max(sphere.tolerance, box.tolerance),
      name);
  if (!result.body) {
    result.diagnostics =
        std::string("boolean ") + op_name(op) + ": failed to build ⅛-ball";
  }
  return result;
}

}  // namespace brep::boolean
