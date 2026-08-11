#include "brep/bool/sphere_box_boolean.hpp"

#include "brep/geometry.hpp"
#include "brep/log.hpp"
#include "brep/validate.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <optional>
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

Edge* make_arc_edge(Model& model, CircleCurve* curve, Vertex* a, Vertex* b,
                    double tol, const std::string& name) {
  double t0 = circle_param_at(*curve, a->position());
  double t1 = circle_param_at(*curve, b->position());
  double forward = norm_angle(t1 - t0);
  if (forward > std::numbers::pi) {
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
  const double d0 = (edge->v0->position() - from->position()).squaredNorm();
  const double d1 = (edge->v1->position() - from->position()).squaredNorm();
  return d0 <= d1 ? Orientation::Forward : Orientation::Reversed;
}

struct CornerOctant {
  Point3d c{};
  double sx{1.0};
  double sy{1.0};
  double sz{1.0};
};

[[nodiscard]] bool point_in_box(const BoxSpec& box, const Point3d& p,
                                double eps) {
  return p.x() >= box.min.x() - eps && p.x() <= box.max.x() + eps &&
         p.y() >= box.min.y() - eps && p.y() <= box.max.y() + eps &&
         p.z() >= box.min.z() - eps && p.z() <= box.max.z() + eps;
}

[[nodiscard]] std::optional<CornerOctant> detect_corner_octant(
    const SphereSpec& sphere, const BoxSpec& box, double eps) {
  const Point3d& c = sphere.center;
  const double r = sphere.radius;
  const Point3d corners[8] = {
      {box.min.x(), box.min.y(), box.min.z()},
      {box.max.x(), box.min.y(), box.min.z()},
      {box.min.x(), box.max.y(), box.min.z()},
      {box.max.x(), box.max.y(), box.min.z()},
      {box.min.x(), box.min.y(), box.max.z()},
      {box.max.x(), box.min.y(), box.max.z()},
      {box.min.x(), box.max.y(), box.max.z()},
      {box.max.x(), box.max.y(), box.max.z()},
  };

  for (const Point3d& corner : corners) {
    if (std::abs(c.x() - corner.x()) > eps ||
        std::abs(c.y() - corner.y()) > eps ||
        std::abs(c.z() - corner.z()) > eps) {
      continue;
    }
    CornerOctant pose;
    pose.c = c;
    pose.sx = (std::abs(corner.x() - box.min.x()) <= eps) ? 1.0 : -1.0;
    pose.sy = (std::abs(corner.y() - box.min.y()) <= eps) ? 1.0 : -1.0;
    pose.sz = (std::abs(corner.z() - box.min.z()) <= eps) ? 1.0 : -1.0;

    const Point3d ax{c.x() + pose.sx * r, c.y(), c.z()};
    const Point3d ay{c.x(), c.y() + pose.sy * r, c.z()};
    const Point3d az{c.x(), c.y(), c.z() + pose.sz * r};
    const Point3d far{c.x() + pose.sx * r, c.y() + pose.sy * r,
                      c.z() + pose.sz * r};
    if (point_in_box(box, ax, eps) && point_in_box(box, ay, eps) &&
        point_in_box(box, az, eps) && point_in_box(box, far, eps)) {
      return pose;
    }
  }
  return std::nullopt;
}

/// `seven_eighths==false` → inward ⅛ ball; true → sphere minus that octant.
Body* build_axis_octant_ball(Model& model, const CornerOctant& pose, double r,
                             double tol, const std::string& name,
                             bool seven_eighths) {
  const Point3d& c = pose.c;
  const Point3d ax{c.x() + pose.sx * r, c.y(), c.z()};
  const Point3d ay{c.x(), c.y() + pose.sy * r, c.z()};
  const Point3d az{c.x(), c.y(), c.z() + pose.sz * r};

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
      model.make_circle(c, Vector3d{0, 0, pose.sx * pose.sy}, r, name + "_cxy");
  CircleCurve* c_yz =
      model.make_circle(c, Vector3d{pose.sy * pose.sz, 0, 0}, r, name + "_cyz");
  CircleCurve* c_zx =
      model.make_circle(c, Vector3d{0, pose.sz * pose.sx, 0}, r, name + "_czx");

  Edge* e_xy = make_arc_edge(model, c_xy, vx, vy, tol, name + "_exy");
  Edge* e_yz = make_arc_edge(model, c_yz, vy, vz, tol, name + "_eyz");
  Edge* e_zx = make_arc_edge(model, c_zx, vz, vx, tol, name + "_ezx");

  Body* body = model.make_body(BodyType::Solid, name);
  Shell* shell = model.make_shell(true, name + "_shell");
  body->shells.push_back(shell);

  const Orientation plane_sense =
      seven_eighths ? Orientation::Forward : Orientation::Reversed;

  auto add_plane_face = [&](Vector3d u, Vector3d v, Edge* e0, Vertex* a0,
                            Vertex* b0, Edge* e1, Vertex* a1, Vertex* b1,
                            Edge* e2, Vertex* a2, Vertex* b2,
                            const std::string& fname) {
    PlaneSurface* surf = model.make_plane(c, u, v, fname);
    Face* face = model.make_face(surf, plane_sense, fname);
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, fname + "_outer");
    if (seven_eighths) {
      CoEdge* c0 = model.make_coedge(e2, sense_along(e2, b2, a2));
      CoEdge* c1 = model.make_coedge(e1, sense_along(e1, b1, a1));
      CoEdge* c2 = model.make_coedge(e0, sense_along(e0, b0, a0));
      Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    } else {
      CoEdge* c0 = model.make_coedge(e0, sense_along(e0, a0, b0));
      CoEdge* c1 = model.make_coedge(e1, sense_along(e1, a1, b1));
      CoEdge* c2 = model.make_coedge(e2, sense_along(e2, a2, b2));
      Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    }
  };

  // Planes through C spanning inward axes (u×v points along +inward third axis
  // before face sense). Canonical +++ used u,v → +Z then Reversed → −Z outward
  // for ⅛. With signs, u = sx*X-hat etc.
  const Vector3d ex{pose.sx, 0, 0};
  const Vector3d ey{0, pose.sy, 0};
  const Vector3d ez{0, 0, pose.sz};

  add_plane_face(ex, ey, e_ox, vo, vx, e_xy, vx, vy, e_oy, vy, vo, name + "_fz");
  add_plane_face(ey, ez, e_oy, vo, vy, e_yz, vy, vz, e_oz, vz, vo, name + "_fx");
  add_plane_face(ez, ex, e_oz, vo, vz, e_zx, vz, vx, e_ox, vx, vo, name + "_fy");

  {
    SphereSurface* surf = model.make_sphere_surface(c, r, name + "_fs");
    Face* face = model.make_face(surf, Orientation::Forward, name + "_fs");
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, name + "_fs_outer");
    if (seven_eighths) {
      CoEdge* c0 = model.make_coedge(e_xy, sense_along(e_xy, vx, vy));
      CoEdge* c1 = model.make_coedge(e_yz, sense_along(e_yz, vy, vz));
      CoEdge* c2 = model.make_coedge(e_zx, sense_along(e_zx, vz, vx));
      Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    } else {
      CoEdge* c0 = model.make_coedge(e_zx, sense_along(e_zx, vx, vz));
      CoEdge* c1 = model.make_coedge(e_yz, sense_along(e_yz, vz, vy));
      CoEdge* c2 = model.make_coedge(e_xy, sense_along(e_xy, vy, vx));
      Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    }
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

[[nodiscard]] Face* find_plane_face(Body& body, const Point3d& origin,
                                    const Vector3d& outward, double eps) {
  const Vector3d n = outward.normalized();
  for (Shell* shell : body.shells) {
    if (!shell) continue;
    for (Face* face : shell->faces) {
      if (!face || !face->surface ||
          face->surface->kind() != SurfaceKind::Plane) {
        continue;
      }
      const auto& pl = static_cast<const PlaneSurface&>(*face->surface);
      Vector3d fn = pl.normal(0, 0);
      if (face->sense == Orientation::Reversed) fn = -fn;
      if (fn.dot(n) < 1.0 - 1e-6) continue;
      const Vector3d d = origin - pl.origin();
      if (std::abs(d.dot(fn)) > eps) continue;
      return face;
    }
  }
  return nullptr;
}

/// Box ∪ sphere when sphere center is at a box corner and the inward octant of
/// the ball lies inside the box. Topology: box + spherical ⅞ face + 3 Inner
/// quarter-circle holes on the corner faces.
Body* build_sphere_box_union(Model& model, const CornerOctant& pose,
                             const BoxSpec& box, double r, double tol,
                             const std::string& name) {
  BoxSpec box_copy = box;
  box_copy.name = name + "_box";
  Body* body = make_box(model, box_copy);
  if (!body || body->shells.empty()) return nullptr;
  Shell* shell = body->shells.front();
  body->name = name;

  const Point3d& c = pose.c;
  Vertex* vo = nullptr;
  for (Shell* sh : body->shells) {
    for (Face* f : sh->faces) {
      for (Loop* lp : f->loops) {
        if (!lp) continue;
        lp->for_each_coedge([&](const CoEdge& ce) {
          if (Vertex* v = ce.from()) {
            const Point3d& p = v->position();
            if (std::abs(p.x() - c.x()) <= tol &&
                std::abs(p.y() - c.y()) <= tol &&
                std::abs(p.z() - c.z()) <= tol) {
              vo = v;
            }
          }
        });
      }
    }
  }
  if (!vo) {
    BREP_ERROR("sphere-box union: corner vertex not found");
    return nullptr;
  }

  const Point3d ax{c.x() + pose.sx * r, c.y(), c.z()};
  const Point3d ay{c.x(), c.y() + pose.sy * r, c.z()};
  const Point3d az{c.x(), c.y(), c.z() + pose.sz * r};
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
      model.make_circle(c, Vector3d{0, 0, pose.sx * pose.sy}, r, name + "_cxy");
  CircleCurve* c_yz =
      model.make_circle(c, Vector3d{pose.sy * pose.sz, 0, 0}, r, name + "_cyz");
  CircleCurve* c_zx =
      model.make_circle(c, Vector3d{0, pose.sz * pose.sx, 0}, r, name + "_czx");
  Edge* e_xy = make_arc_edge(model, c_xy, vx, vy, tol, name + "_exy");
  Edge* e_yz = make_arc_edge(model, c_yz, vy, vz, tol, name + "_eyz");
  Edge* e_zx = make_arc_edge(model, c_zx, vz, vx, tol, name + "_ezx");

  // Outward normals of the three box faces at the corner (point out of box).
  const Vector3d out_x{-pose.sx, 0, 0};
  const Vector3d out_y{0, -pose.sy, 0};
  const Vector3d out_z{0, 0, -pose.sz};
  Face* fx = find_plane_face(*body, c, out_x, tol);
  Face* fy = find_plane_face(*body, c, out_y, tol);
  Face* fz = find_plane_face(*body, c, out_z, tol);
  if (!fx || !fy || !fz) {
    BREP_ERROR("sphere-box union: corner faces not found");
    return nullptr;
  }

  auto add_inner = [&](Face* face, Edge* e0, Vertex* a0, Vertex* b0, Edge* e1,
                       Vertex* a1, Vertex* b1, Edge* e2, Vertex* a2, Vertex* b2,
                       const std::string& lname) {
    Loop* loop = model.make_loop(face, LoopType::Inner, lname);
    // Same reverse order as ⅞ planar faces: hole leaves material outside.
    CoEdge* c0 = model.make_coedge(e2, sense_along(e2, b2, a2));
    CoEdge* c1 = model.make_coedge(e1, sense_along(e1, b1, a1));
    CoEdge* c2 = model.make_coedge(e0, sense_along(e0, b0, a0));
    Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
  };

  // Face z (out_z): hole O→Ay→Ax→O (reverse of O→Ax→Ay→O).
  add_inner(fz, e_ox, vo, vx, e_xy, vx, vy, e_oy, vy, vo, name + "_fz_inner");
  // Face x: O→Az→Ay→O
  add_inner(fx, e_oy, vo, vy, e_yz, vy, vz, e_oz, vz, vo, name + "_fx_inner");
  // Face y: O→Ax→Az→O
  add_inner(fy, e_oz, vo, vz, e_zx, vz, vx, e_ox, vx, vo, name + "_fy_inner");

  {
    SphereSurface* surf = model.make_sphere_surface(c, r, name + "_fs");
    Face* face = model.make_face(surf, Orientation::Forward, name + "_fs");
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, name + "_fs_outer");
    // ⅞-style winding (exterior spherical patch).
    CoEdge* c0 = model.make_coedge(e_xy, sense_along(e_xy, vx, vy));
    CoEdge* c1 = model.make_coedge(e_yz, sense_along(e_yz, vy, vz));
    CoEdge* c2 = model.make_coedge(e_zx, sense_along(e_zx, vz, vx));
    Model::link_loop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
  }

  for (Edge* e : {e_ox, e_oy, e_oz, e_xy, e_yz, e_zx}) {
    if (e->radial.size() != 2) {
      BREP_ERROR("sphere-box union: edge '{}' radial={}", e->name,
                 e->radial.size());
      return nullptr;
    }
    Model::pair_partners(e->radial[0], e->radial[1]);
  }

  const auto report = validate_body(*body);
  if (!report.ok()) {
    for (const auto& issue : report.issues) {
      BREP_WARN("sphere-box union validate [{}] {}", issue.where, issue.message);
    }
    return nullptr;
  }
  return body;
}

[[nodiscard]] bool sphere_inside_box(const SphereSpec& s, const BoxSpec& b,
                                     double eps) {
  return s.center.x() - s.radius >= b.min.x() - eps &&
         s.center.x() + s.radius <= b.max.x() + eps &&
         s.center.y() - s.radius >= b.min.y() - eps &&
         s.center.y() + s.radius <= b.max.y() + eps &&
         s.center.z() - s.radius >= b.min.z() - eps &&
         s.center.z() + s.radius <= b.max.z() + eps;
}

[[nodiscard]] bool box_inside_sphere(const BoxSpec& b, const SphereSpec& s,
                                     double eps) {
  const Point3d corners[8] = {
      {b.min.x(), b.min.y(), b.min.z()}, {b.max.x(), b.min.y(), b.min.z()},
      {b.min.x(), b.max.y(), b.min.z()}, {b.max.x(), b.max.y(), b.min.z()},
      {b.min.x(), b.min.y(), b.max.z()}, {b.max.x(), b.min.y(), b.max.z()},
      {b.min.x(), b.max.y(), b.max.z()}, {b.max.x(), b.max.y(), b.max.z()},
  };
  for (const Point3d& p : corners) {
    if ((p - s.center).norm() > s.radius + eps) return false;
  }
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
  const double tol = std::max(sphere.tolerance, box.tolerance);
  const auto pose = detect_corner_octant(sphere, box, eps);

  if (op == BooleanOp::Intersect) {
    if (!pose) {
      result.diagnostics =
          "boolean Intersect: sphere–box requires sphere center at a box "
          "corner with the inward octant of the ball inside the box";
      BREP_WARN("{}", result.diagnostics);
      return result;
    }
    result.body = build_axis_octant_ball(
        model, *pose, sphere.radius, tol,
        std::string("bool_sphere_box_") + op_name(op), /*seven_eighths=*/false);
    if (!result.body) {
      result.diagnostics =
          std::string("boolean ") + op_name(op) + ": failed to build ⅛-ball";
    }
    return result;
  }

  if (op == BooleanOp::Subtract && sphere_is_a) {
    if (!pose) {
      result.diagnostics =
          "boolean Subtract: Sphere−Box currently requires sphere center at a "
          "box corner with the inward octant inside the box (⅞-ball)";
      BREP_WARN("{}", result.diagnostics);
      return result;
    }
    result.body = build_axis_octant_ball(
        model, *pose, sphere.radius, tol,
        std::string("bool_sphere_box_") + op_name(op), /*seven_eighths=*/true);
    if (!result.body) {
      result.diagnostics =
          "boolean Subtract: failed to build Sphere−Box (⅞-ball)";
    }
    return result;
  }

  if (op == BooleanOp::Subtract && !sphere_is_a) {
    result.diagnostics =
        "boolean Subtract: Box−Sphere is not implemented yet (T3.7 covers "
        "Sphere−Box only)";
    BREP_WARN("{}", result.diagnostics);
    return result;
  }

  if (op == BooleanOp::Union) {
    if (sphere_inside_box(sphere, box, eps)) {
      BoxSpec copy = box;
      copy.name = "bool_sphere_box_Union";
      result.body = make_box(model, copy);
      return result;
    }
    if (box_inside_sphere(box, sphere, eps)) {
      SphereSpec copy = sphere;
      copy.name = "bool_sphere_box_Union";
      result.body = make_sphere(model, copy);
      return result;
    }
    if (!pose) {
      result.diagnostics =
          "boolean Union: sphere–box requires containment or sphere center at "
          "a box corner with the inward octant inside the box";
      BREP_WARN("{}", result.diagnostics);
      return result;
    }
    result.body = build_sphere_box_union(model, *pose, box, sphere.radius, tol,
                                        "bool_sphere_box_Union");
    if (!result.body) {
      result.diagnostics = "boolean Union: failed to build Sphere∪Box shell";
    }
    return result;
  }

  result.diagnostics =
      std::string("boolean ") + op_name(op) +
      ": sphere–box unsupported operation";
  BREP_WARN("{}", result.diagnostics);
  return result;
}

}  // namespace brep::boolean
