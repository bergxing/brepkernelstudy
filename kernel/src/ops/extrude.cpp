#include "brep/ops/profile.hpp"

#include "brep/builder.hpp"
#include "brep/log.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace brep::ops {
namespace {

bool is_axis_aligned_rect(const Profile2d& profile, double& min_u, double& max_u,
                          double& min_v, double& max_v) {
  if (profile.outer.size() != 4) return false;
  min_u = max_u = profile.outer[0].u();
  min_v = max_v = profile.outer[0].v();
  for (const auto& p : profile.outer) {
    min_u = std::min(min_u, p.u());
    max_u = std::max(max_u, p.u());
    min_v = std::min(min_v, p.v());
    max_v = std::max(max_v, p.v());
  }
  int corners = 0;
  for (const auto& p : profile.outer) {
    const bool on_u = std::abs(p.u() - min_u) < 1e-9 ||
                      std::abs(p.u() - max_u) < 1e-9;
    const bool on_v = std::abs(p.v() - min_v) < 1e-9 ||
                      std::abs(p.v() - max_v) < 1e-9;
    if (on_u && on_v) ++corners;
  }
  return corners == 4 && (max_u - min_u) > 1e-9 && (max_v - min_v) > 1e-9;
}

/// Build a solid prism for a convex polygon in plane, extruded along normal.
Body* extrude_polygon(Model& model, const ExtrudeSpec& spec) {
  const auto& poly = spec.profile.outer;
  if (poly.size() < 3) return nullptr;

  const double d0 = spec.symmetric ? -0.5 * spec.distance : 0.0;
  const double d1 = spec.symmetric ? 0.5 * spec.distance : spec.distance;
  const std::size_t n = poly.size();

  std::vector<Vertex*> bottom_v;
  std::vector<Vertex*> top_v;
  bottom_v.reserve(n);
  top_v.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    const Point3d b =
        spec.plane.to_world(poly[i]) + spec.plane.normal * d0;
    const Point3d t =
        spec.plane.to_world(poly[i]) + spec.plane.normal * d1;
    bottom_v.push_back(model.make_vertex(model.make_point(b), spec.tolerance));
    top_v.push_back(model.make_vertex(model.make_point(t), spec.tolerance));
  }

  Body* body = model.make_body(BodyType::Solid, spec.name);
  Shell* shell = model.make_shell(true, spec.name + "_shell");
  body->shells.push_back(shell);

  auto make_side_edge = [&](Vertex* a, Vertex* b, const std::string& name) {
    LineCurve* curve =
        model.make_line(a->position(), b->position(), name + "_crv");
    return model.make_edge(curve, a, b, 0.0, (b->position() - a->position()).norm(),
                           spec.tolerance, name);
  };

  std::vector<Edge*> bottom_e(n);
  std::vector<Edge*> top_e(n);
  std::vector<Edge*> vert_e(n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t j = (i + 1) % n;
    bottom_e[i] =
        make_side_edge(bottom_v[i], bottom_v[j], spec.name + "_be" + std::to_string(i));
    top_e[i] =
        make_side_edge(top_v[i], top_v[j], spec.name + "_te" + std::to_string(i));
    vert_e[i] =
        make_side_edge(bottom_v[i], top_v[i], spec.name + "_ve" + std::to_string(i));
  }

  auto add_quad_face = [&](Edge* e0, Orientation s0, Edge* e1, Orientation s1,
                           Edge* e2, Orientation s2, Edge* e3, Orientation s3,
                           Point3d origin, Vector3d u, Vector3d v,
                           const std::string& name) {
    PlaneSurface* surf = model.make_plane(origin, u, v, name);
    Face* face = model.make_face(surf, Orientation::Forward, name);
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, name + "_outer");
    CoEdge* c0 = model.make_coedge(e0, s0, nullptr, name + "_c0");
    CoEdge* c1 = model.make_coedge(e1, s1, nullptr, name + "_c1");
    CoEdge* c2 = model.make_coedge(e2, s2, nullptr, name + "_c2");
    CoEdge* c3 = model.make_coedge(e3, s3, nullptr, name + "_c3");
    Model::link_loop(loop, std::array<CoEdge*, 4>{c0, c1, c2, c3});
    return std::array<CoEdge*, 4>{c0, c1, c2, c3};
  };

  // Bottom face (reverse winding relative to normal).
  {
    PlaneSurface* surf = model.make_plane(
        spec.plane.origin + spec.plane.normal * d0, -spec.plane.normal,
        spec.name + "_bottom");
    Face* face = model.make_face(surf, Orientation::Forward, spec.name + "_bottom");
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, "bottom_outer");
    std::vector<CoEdge*> ces;
    ces.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      // Reverse around bottom
      const std::size_t k = n - 1 - i;
      ces.push_back(model.make_coedge(bottom_e[k], Orientation::Reversed));
    }
    Model::link_loop(loop, ces);
  }

  // Top face
  {
    PlaneSurface* surf = model.make_plane(
        spec.plane.origin + spec.plane.normal * d1, spec.plane.normal,
        spec.name + "_top");
    Face* face = model.make_face(surf, Orientation::Forward, spec.name + "_top");
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, "top_outer");
    std::vector<CoEdge*> ces;
    ces.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      ces.push_back(model.make_coedge(top_e[i], Orientation::Forward));
    }
    Model::link_loop(loop, ces);
  }

  // Side faces
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t j = (i + 1) % n;
    const Point3d o = bottom_v[i]->position();
    const Vector3d u =
        (bottom_v[j]->position() - bottom_v[i]->position()).normalized();
    const Vector3d v = spec.plane.normal;
    add_quad_face(bottom_e[i], Orientation::Forward, vert_e[j],
                  Orientation::Forward, top_e[i], Orientation::Reversed,
                  vert_e[i], Orientation::Reversed, o, u, v,
                  spec.name + "_side" + std::to_string(i));
  }

  // Pair radial partners: each edge should have 2 coedges.
  std::unordered_map<Edge*, std::vector<CoEdge*>> by_edge;
  for (Face* f : shell->faces) {
    for (Loop* loop : f->loops) {
      CoEdge* c = loop->first;
      if (!c) continue;
      do {
        by_edge[c->edge].push_back(c);
        c = c->next;
      } while (c && c != loop->first);
    }
  }
  for (auto& [e, ces] : by_edge) {
    (void)e;
    if (ces.size() == 2) Model::pair_partners(ces[0], ces[1]);
  }

  BREP_INFO("extrude_polygon '{}' n={} distance={}", spec.name, n, spec.distance);
  return body;
}

}  // namespace

Profile2d extract_profile(const sketch::Sketch& sketch) {
  Profile2d profile;
  if (sketch.points().empty()) return profile;

  // If we have lines, walk them; else use all points as a polygon in order.
  if (!sketch.lines().empty()) {
    std::vector<bool> used(sketch.lines().size(), false);
    sketch::SketchEntityId cur = sketch.lines().front().p0;
    profile.outer.push_back(sketch.point(cur)->p);
    for (std::size_t guard = 0; guard < sketch.lines().size(); ++guard) {
      bool advanced = false;
      for (std::size_t i = 0; i < sketch.lines().size(); ++i) {
        if (used[i]) continue;
        const auto& ln = sketch.lines()[i];
        if (ln.p0 == cur) {
          cur = ln.p1;
          used[i] = true;
          if (const auto* pt = sketch.point(cur)) profile.outer.push_back(pt->p);
          advanced = true;
          break;
        }
        if (ln.p1 == cur) {
          cur = ln.p0;
          used[i] = true;
          if (const auto* pt = sketch.point(cur)) profile.outer.push_back(pt->p);
          advanced = true;
          break;
        }
      }
      if (!advanced) break;
    }
    // Drop duplicate closing point.
    if (profile.outer.size() >= 2 &&
        std::abs(profile.outer.front().u() - profile.outer.back().u()) < 1e-9 &&
        std::abs(profile.outer.front().v() - profile.outer.back().v()) < 1e-9) {
      profile.outer.pop_back();
    }
  } else {
    for (const auto& p : sketch.points()) profile.outer.push_back(p.p);
  }
  return profile;
}

Body* extrude(Model& model, const ExtrudeSpec& spec) {
  if (spec.profile.outer.size() < 3 || std::abs(spec.distance) < 1e-12) {
    BREP_WARN("extrude: invalid profile or distance");
    return nullptr;
  }

  double min_u = 0, max_u = 0, min_v = 0, max_v = 0;
  if (is_axis_aligned_rect(spec.profile, min_u, max_u, min_v, max_v)) {
    // Map plane UV rectangle + extrude distance into AABB box when plane is
    // xz_y_up (viewer ground).
    const Plane& pl = spec.plane;
    const bool y_up =
        std::abs(pl.normal.y() - 1.0) < 1e-6 &&
        std::abs(pl.u_axis.x() - 1.0) < 1e-6 &&
        std::abs(pl.v_axis.z() - 1.0) < 1e-6;
    if (y_up) {
      const double d0 = spec.symmetric ? -0.5 * spec.distance : 0.0;
      const double d1 = spec.symmetric ? 0.5 * spec.distance : spec.distance;
      const double miny = std::min(d0, d1);
      const double maxy = std::max(d0, d1);
      BoxSpec box;
      box.min = Point3d{min_u, miny, min_v};
      box.max = Point3d{max_u, maxy, max_v};
      box.name = spec.name;
      box.tolerance = spec.tolerance;
      return make_box(model, box);
    }
  }

  return extrude_polygon(model, spec);
}

}  // namespace brep::ops
