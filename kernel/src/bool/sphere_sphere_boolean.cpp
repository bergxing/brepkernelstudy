#include "brep/bool/sphere_sphere_boolean.hpp"

#include "brep/bool/intersect_sphere_sphere.hpp"
#include "brep/geometry.hpp"
#include "brep/log.hpp"
#include "brep/validate.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <string>

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

[[nodiscard]] Orientation sense_along(Edge* edge, Vertex* from, Vertex* to) {
  if (edge->v0 == from && edge->v1 == to) return Orientation::Forward;
  if (edge->v0 == to && edge->v1 == from) return Orientation::Reversed;
  const double d0 = (edge->v0->position() - from->position()).squaredNorm();
  const double d1 = (edge->v1->position() - from->position()).squaredNorm();
  return d0 <= d1 ? Orientation::Forward : Orientation::Reversed;
}

Body* build_two_cap_union(Model& model, const SphereSpec& sa,
                          const SphereSpec& sb, const Point3d& circ_c,
                          const Vector3d& circ_n, double circ_r, double tol,
                          const std::string& name) {
  const Vector3d n = circ_n.normalized();
  const Vector3d ref =
      std::abs(n.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
  const Vector3d x = n.cross(ref).normalized();

  Vertex* v0 =
      model.make_vertex(model.make_point(circ_c + x * circ_r), tol, name + "_v0");
  Vertex* v1 =
      model.make_vertex(model.make_point(circ_c - x * circ_r), tol, name + "_v1");

  CircleCurve* curve = model.make_circle(circ_c, n, circ_r, name + "_circ");
  const double ta = circle_param_at(*curve, v0->position());
  // Antipodal halves: v0 at ta, v1 at ta+π (v0/v1 are opposite on the circle).
  Edge* e0 = model.make_edge(curve, v0, v1, ta, ta + std::numbers::pi, tol,
                             name + "_e0");
  Edge* e1 = model.make_edge(curve, v1, v0, ta + std::numbers::pi,
                             ta + 2.0 * std::numbers::pi, tol, name + "_e1");

  Body* body = model.make_body(BodyType::Solid, name);
  Shell* shell = model.make_shell(true, name + "_shell");
  body->shells.push_back(shell);

  auto add_cap = [&](const SphereSpec& spec, bool reverse_loop,
                     const std::string& fname) {
    SphereSurface* surf =
        model.make_sphere_surface(spec.center, spec.radius, fname);
    Face* face = model.make_face(surf, Orientation::Forward, fname);
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, fname + "_outer");
    if (reverse_loop) {
      CoEdge* c0 = model.make_coedge(e1, sense_along(e1, v0, v1));
      CoEdge* c1 = model.make_coedge(e0, sense_along(e0, v1, v0));
      Model::link_loop(loop, std::array<CoEdge*, 2>{c0, c1});
    } else {
      CoEdge* c0 = model.make_coedge(e0, sense_along(e0, v0, v1));
      CoEdge* c1 = model.make_coedge(e1, sense_along(e1, v1, v0));
      Model::link_loop(loop, std::array<CoEdge*, 2>{c0, c1});
    }
  };

  add_cap(sa, false, name + "_fa");
  add_cap(sb, true, name + "_fb");

  for (Edge* e : {e0, e1}) {
    if (e->radial.size() != 2) {
      BREP_ERROR("sphere union: edge '{}' radial={}", e->name, e->radial.size());
      return nullptr;
    }
    Model::pair_partners(e->radial[0], e->radial[1]);
  }

  const auto report = validate_body(*body);
  if (!report.ok()) {
    for (const auto& issue : report.issues) {
      BREP_WARN("sphere union validate [{}] {}", issue.where, issue.message);
    }
    return nullptr;
  }
  return body;
}

Body* clone_sphere_body(Model& model, const SphereSpec& spec,
                        const std::string& name) {
  SphereSpec copy = spec;
  copy.name = name;
  return make_sphere(model, copy);
}

}  // namespace

BooleanResult evaluate_sphere_sphere_boolean(BooleanOp op, Model& model,
                                             const SphereSpec& a,
                                             const SphereSpec& b,
                                             const BooleanContext& ctx) {
  BooleanResult result;
  result.mode = BooleanEvalMode::AnalyticPair;

  if (op != BooleanOp::Union) {
    result.diagnostics =
        std::string("boolean ") + op_name(op) +
        ": sphere–sphere currently supports Union only (T3.8)";
    BREP_WARN("{}", result.diagnostics);
    return result;
  }

  const auto ix =
      intersect_sphere_sphere(a.center, a.radius, b.center, b.radius, ctx);
  const double tol = std::max(a.tolerance, b.tolerance);
  const std::string name = "bool_sphere_sphere_Union";

  switch (ix.status) {
    case SphereSphereStatus::Circle: {
      result.body = build_two_cap_union(model, a, b, ix.center, ix.normal,
                                       ix.radius, tol, name);
      if (!result.body) {
        result.diagnostics =
            "boolean Union: failed to build intersecting Sphere∪Sphere shell";
      }
      return result;
    }
    case SphereSphereStatus::Contained: {
      const SphereSpec& larger = a.radius >= b.radius ? a : b;
      result.body = clone_sphere_body(model, larger, name);
      if (!result.body) {
        result.diagnostics = "boolean Union: failed to clone containing sphere";
      }
      return result;
    }
    case SphereSphereStatus::Coincident: {
      result.body = clone_sphere_body(model, a, name);
      if (!result.body) {
        result.diagnostics = "boolean Union: failed to clone coincident sphere";
      }
      return result;
    }
    case SphereSphereStatus::Point: {
      result.diagnostics =
          "boolean Union: sphere–sphere tangency (point) not built as a single "
          "manifold yet";
      BREP_WARN("{}", result.diagnostics);
      return result;
    }
    case SphereSphereStatus::Separate:
    default:
      result.diagnostics =
          "boolean Union: sphere–sphere separate — disconnected multi-body "
          "union not implemented";
      BREP_WARN("{}", result.diagnostics);
      return result;
  }
}

}  // namespace brep::boolean
