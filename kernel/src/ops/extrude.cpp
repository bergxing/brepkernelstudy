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

struct RingGeom {
  std::vector<Vertex*> bottom_v;
  std::vector<Vertex*> top_v;
  std::vector<Edge*> bottom_e;
  std::vector<Edge*> top_e;
  std::vector<Edge*> vert_e;
  bool is_hole{false};
};

RingGeom build_ring(Model& model, const ExtrudeSpec& spec,
                    const std::vector<Point2d>& poly, bool is_hole,
                    const std::string& tag) {
  RingGeom ring;
  ring.is_hole = is_hole;
  const double d0 = spec.symmetric ? -0.5 * spec.distance : 0.0;
  const double d1 = spec.symmetric ? 0.5 * spec.distance : spec.distance;
  const std::size_t n = poly.size();
  ring.bottom_v.reserve(n);
  ring.top_v.reserve(n);
  ring.bottom_e.resize(n);
  ring.top_e.resize(n);
  ring.vert_e.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    const Point3d b =
        spec.plane.to_world(poly[i]) + spec.plane.normal * d0;
    const Point3d t =
        spec.plane.to_world(poly[i]) + spec.plane.normal * d1;
    ring.bottom_v.push_back(
        model.make_vertex(model.make_point(b), spec.tolerance));
    ring.top_v.push_back(model.make_vertex(model.make_point(t), spec.tolerance));
  }

  auto make_side_edge = [&](Vertex* a, Vertex* b, const std::string& name) {
    LineCurve* curve =
        model.make_line(a->position(), b->position(), name + "_crv");
    return model.make_edge(curve, a, b, 0.0,
                           (b->position() - a->position()).norm(),
                           spec.tolerance, name);
  };

  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t j = (i + 1) % n;
    ring.bottom_e[i] = make_side_edge(
        ring.bottom_v[i], ring.bottom_v[j], tag + "_be" + std::to_string(i));
    ring.top_e[i] = make_side_edge(ring.top_v[i], ring.top_v[j],
                                   tag + "_te" + std::to_string(i));
    ring.vert_e[i] = make_side_edge(ring.bottom_v[i], ring.top_v[i],
                                    tag + "_ve" + std::to_string(i));
  }
  return ring;
}

void add_quad_face(Model& model, Shell* shell, Edge* e0, Orientation s0,
                   Edge* e1, Orientation s1, Edge* e2, Orientation s2,
                   Edge* e3, Orientation s3, Point3d origin, Vector3d u,
                   Vector3d v, const std::string& name) {
  PlaneSurface* surf = model.make_plane(origin, u, v, name);
  Face* face = model.make_face(surf, Orientation::Forward, name);
  shell->faces.push_back(face);
  Loop* loop = model.make_loop(face, LoopType::Outer, name + "_outer");
  CoEdge* c0 = model.make_coedge(e0, s0, nullptr, name + "_c0");
  CoEdge* c1 = model.make_coedge(e1, s1, nullptr, name + "_c1");
  CoEdge* c2 = model.make_coedge(e2, s2, nullptr, name + "_c2");
  CoEdge* c3 = model.make_coedge(e3, s3, nullptr, name + "_c3");
  Model::link_loop(loop, std::array<CoEdge*, 4>{c0, c1, c2, c3});
}

/// Build a solid prism for a polygon (optional holes) in plane, extruded along
/// normal.
Body* extrude_polygon(Model& model, const ExtrudeSpec& spec) {
  const auto& poly = spec.profile.outer;
  if (poly.size() < 3) return nullptr;

  const double d0 = spec.symmetric ? -0.5 * spec.distance : 0.0;
  const double d1 = spec.symmetric ? 0.5 * spec.distance : spec.distance;

  Body* body = model.make_body(BodyType::Solid, spec.name);
  Shell* shell = model.make_shell(true, spec.name + "_shell");
  body->shells.push_back(shell);

  std::vector<RingGeom> rings;
  rings.push_back(build_ring(model, spec, poly, false, spec.name + "_outer"));
  for (std::size_t hi = 0; hi < spec.profile.holes.size(); ++hi) {
    const auto& hole = spec.profile.holes[hi];
    if (hole.size() < 3) continue;
    rings.push_back(build_ring(model, spec, hole, true,
                               spec.name + "_hole" + std::to_string(hi)));
  }

  // Bottom face (outward normal = -plane.normal): Outer reversed; Inners
  // opposite to Outer.
  {
    PlaneSurface* surf = model.make_plane(
        spec.plane.origin + spec.plane.normal * d0, -spec.plane.normal,
        spec.name + "_bottom");
    Face* face =
        model.make_face(surf, Orientation::Forward, spec.name + "_bottom");
    shell->faces.push_back(face);

    for (const RingGeom& ring : rings) {
      const std::size_t n = ring.bottom_e.size();
      Loop* loop = model.make_loop(
          face, ring.is_hole ? LoopType::Inner : LoopType::Outer,
          ring.is_hole ? "bottom_inner" : "bottom_outer");
      std::vector<CoEdge*> ces;
      ces.reserve(n);
      if (!ring.is_hole) {
        for (std::size_t i = 0; i < n; ++i) {
          const std::size_t k = n - 1 - i;
          ces.push_back(
              model.make_coedge(ring.bottom_e[k], Orientation::Reversed));
        }
      } else {
        // Opposite winding to Outer on the same face.
        for (std::size_t i = 0; i < n; ++i) {
          ces.push_back(
              model.make_coedge(ring.bottom_e[i], Orientation::Forward));
        }
      }
      Model::link_loop(loop, ces);
    }
  }

  // Top face (outward = +plane.normal)
  {
    PlaneSurface* surf = model.make_plane(
        spec.plane.origin + spec.plane.normal * d1, spec.plane.normal,
        spec.name + "_top");
    Face* face =
        model.make_face(surf, Orientation::Forward, spec.name + "_top");
    shell->faces.push_back(face);

    for (const RingGeom& ring : rings) {
      const std::size_t n = ring.top_e.size();
      Loop* loop = model.make_loop(
          face, ring.is_hole ? LoopType::Inner : LoopType::Outer,
          ring.is_hole ? "top_inner" : "top_outer");
      std::vector<CoEdge*> ces;
      ces.reserve(n);
      if (!ring.is_hole) {
        for (std::size_t i = 0; i < n; ++i) {
          ces.push_back(
              model.make_coedge(ring.top_e[i], Orientation::Forward));
        }
      } else {
        for (std::size_t i = 0; i < n; ++i) {
          const std::size_t k = n - 1 - i;
          ces.push_back(
              model.make_coedge(ring.top_e[k], Orientation::Reversed));
        }
      }
      Model::link_loop(loop, ces);
    }
  }

  // Side faces: outer walls face outward; hole walls face into the cavity.
  for (std::size_t ri = 0; ri < rings.size(); ++ri) {
    const RingGeom& ring = rings[ri];
    const std::size_t n = ring.bottom_v.size();
    for (std::size_t i = 0; i < n; ++i) {
      const std::size_t j = (i + 1) % n;
      if (!ring.is_hole) {
        const Point3d o = ring.bottom_v[i]->position();
        const Vector3d u =
            (ring.bottom_v[j]->position() - ring.bottom_v[i]->position())
                .normalized();
        const Vector3d v = spec.plane.normal;
        add_quad_face(model, shell, ring.bottom_e[i], Orientation::Forward,
                      ring.vert_e[j], Orientation::Forward, ring.top_e[i],
                      Orientation::Reversed, ring.vert_e[i],
                      Orientation::Reversed, o, u, v,
                      spec.name + "_side" + std::to_string(i));
      } else {
        const Point3d o = ring.bottom_v[j]->position();
        const Vector3d u =
            (ring.bottom_v[i]->position() - ring.bottom_v[j]->position())
                .normalized();
        const Vector3d v = spec.plane.normal;
        add_quad_face(model, shell, ring.bottom_e[i], Orientation::Reversed,
                      ring.vert_e[i], Orientation::Forward, ring.top_e[i],
                      Orientation::Forward, ring.vert_e[j],
                      Orientation::Reversed, o, u, v,
                      spec.name + "_h" + std::to_string(ri) + "_side" +
                          std::to_string(i));
      }
    }
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

  BREP_INFO("extrude_polygon '{}' n={} holes={} distance={}", spec.name,
            poly.size(), rings.size() - 1, spec.distance);
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

  // Holes require prism path (box builder has no Inner loops).
  if (spec.profile.holes.empty()) {
    double min_u = 0, max_u = 0, min_v = 0, max_v = 0;
    if (is_axis_aligned_rect(spec.profile, min_u, max_u, min_v, max_v)) {
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
  }

  return extrude_polygon(model, spec);
}

}  // namespace brep::ops
