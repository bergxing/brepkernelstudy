#include "brep/ops/Profile.h"

#include "brep/Builder.h"
#include "brep/Log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace brep::ops
{
namespace
{

bool is_axis_aligned_rect(const Profile2d& profile, double& min_u, double& max_u,
                          double& min_v, double& max_v)
{
  if (profile.Outer.size() != 4) return false;
  min_u = max_u = profile.Outer[0].u();
  min_v = max_v = profile.Outer[0].v();
  for (const auto& p : profile.Outer)
  {
    min_u = std::min(min_u, p.u());
    max_u = std::max(max_u, p.u());
    min_v = std::min(min_v, p.v());
    max_v = std::max(max_v, p.v());
  }
  int corners = 0;
  for (const auto& p : profile.Outer)
  {
    const bool on_u = std::abs(p.u() - min_u) < 1e-9 ||
                      std::abs(p.u() - max_u) < 1e-9;
    const bool on_v = std::abs(p.v() - min_v) < 1e-9 ||
                      std::abs(p.v() - max_v) < 1e-9;
    if (on_u && on_v) ++corners;
  }
  return corners == 4 && (max_u - min_u) > 1e-9 && (max_v - min_v) > 1e-9;
}

struct RingGeom
{
  std::vector<Vertex*> bottom_v;
  std::vector<Vertex*> top_v;
  std::vector<Edge*> bottom_e;
  std::vector<Edge*> top_e;
  std::vector<Edge*> vert_e;
  bool is_hole{false};
};

RingGeom build_ring(Model& model, const ExtrudeSpec& spec,
                    const std::vector<Point2d>& poly, bool is_hole,
                    const std::string& tag)
                    {
  RingGeom ring;
  ring.is_hole = is_hole;
  const double d0 = spec.Symmetric ? -0.5 * spec.Distance : 0.0;
  const double d1 = spec.Symmetric ? 0.5 * spec.Distance : spec.Distance;
  const std::size_t n = poly.size();
  ring.bottom_v.reserve(n);
  ring.top_v.reserve(n);
  ring.bottom_e.resize(n);
  ring.top_e.resize(n);
  ring.vert_e.resize(n);

  for (std::size_t i = 0; i < n; ++i)
  {
    const Point3d b =
        spec.Plane.ToWorld(poly[i]) + spec.Plane.Normal * d0;
    const Point3d t =
        spec.Plane.ToWorld(poly[i]) + spec.Plane.Normal * d1;
    ring.bottom_v.push_back(
        model.MakeVertex(model.MakePoint(b), spec.Tolerance));
    ring.top_v.push_back(model.MakeVertex(model.MakePoint(t), spec.Tolerance));
  }

  auto make_side_edge = [&](Vertex* a, Vertex* b, const std::string& name)
  {
    LineCurve* curve =
        model.MakeLine(a->Position(), b->Position(), name + "_crv");
    return model.MakeEdge(curve, a, b, 0.0,
                           (b->Position() - a->Position()).norm(),
                           spec.Tolerance, name);
  };

  for (std::size_t i = 0; i < n; ++i)
  {
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
                   Vector3d v, const std::string& name)
                   {
  PlaneSurface* surf = model.MakePlane(origin, u, v, name);
  Face* face = model.MakeFace(surf, Orientation::Forward, name);
  shell->Faces.push_back(face);
  Loop* loop = model.MakeLoop(face, LoopType::Outer, name + "_outer");
  CoEdge* c0 = model.MakeCoedge(e0, s0, nullptr, name + "_c0");
  CoEdge* c1 = model.MakeCoedge(e1, s1, nullptr, name + "_c1");
  CoEdge* c2 = model.MakeCoedge(e2, s2, nullptr, name + "_c2");
  CoEdge* c3 = model.MakeCoedge(e3, s3, nullptr, name + "_c3");
  Model::LinkLoop(loop, std::array<CoEdge*, 4>{c0, c1, c2, c3});
}

/// Build a solid prism for a polygon (optional holes) in plane, extruded along
/// normal.
Body* extrude_polygon(Model& model, const ExtrudeSpec& spec)
{
  const auto& poly = spec.Profile.Outer;
  if (poly.size() < 3) return nullptr;

  const double d0 = spec.Symmetric ? -0.5 * spec.Distance : 0.0;
  const double d1 = spec.Symmetric ? 0.5 * spec.Distance : spec.Distance;

  Body* body = model.MakeBody(BodyType::Solid, spec.Name);
  Shell* shell = model.MakeShell(true, spec.Name + "_shell");
  body->Shells.push_back(shell);

  std::vector<RingGeom> rings;
  rings.push_back(build_ring(model, spec, poly, false, spec.Name + "_outer"));
  for (std::size_t hi = 0; hi < spec.Profile.Holes.size(); ++hi)
  {
    const auto& hole = spec.Profile.Holes[hi];
    if (hole.size() < 3) continue;
    rings.push_back(build_ring(model, spec, hole, true,
                               spec.Name + "_hole" + std::to_string(hi)));
  }

  // Bottom face (outward normal = -plane.Normal): Outer reversed; Inners
  // opposite to Outer.
  {
    PlaneSurface* surf = model.MakePlane(
        spec.Plane.Origin + spec.Plane.Normal * d0, -spec.Plane.Normal,
        spec.Name + "_bottom");
    Face* face =
        model.MakeFace(surf, Orientation::Forward, spec.Name + "_bottom");
    shell->Faces.push_back(face);

    for (const RingGeom& ring : rings)
    {
      const std::size_t n = ring.bottom_e.size();
      Loop* loop = model.MakeLoop(
          face, ring.is_hole ? LoopType::Inner : LoopType::Outer,
          ring.is_hole ? "bottom_inner" : "bottom_outer");
      std::vector<CoEdge*> ces;
      ces.reserve(n);
      if (!ring.is_hole)
      {
        for (std::size_t i = 0; i < n; ++i)
      {
          const std::size_t k = n - 1 - i;
          ces.push_back(
              model.MakeCoedge(ring.bottom_e[k], Orientation::Reversed));
        }
      }
      else
      {
        // Opposite winding to Outer on the same face.
        for (std::size_t i = 0; i < n; ++i)
        {
          ces.push_back(
              model.MakeCoedge(ring.bottom_e[i], Orientation::Forward));
        }
      }
      Model::LinkLoop(loop, ces);
    }
  }

  // Top face (outward = +plane.Normal)
  {
    PlaneSurface* surf = model.MakePlane(
        spec.Plane.Origin + spec.Plane.Normal * d1, spec.Plane.Normal,
        spec.Name + "_top");
    Face* face =
        model.MakeFace(surf, Orientation::Forward, spec.Name + "_top");
    shell->Faces.push_back(face);

    for (const RingGeom& ring : rings)
    {
      const std::size_t n = ring.top_e.size();
      Loop* loop = model.MakeLoop(
          face, ring.is_hole ? LoopType::Inner : LoopType::Outer,
          ring.is_hole ? "top_inner" : "top_outer");
      std::vector<CoEdge*> ces;
      ces.reserve(n);
      if (!ring.is_hole)
      {
        for (std::size_t i = 0; i < n; ++i)
      {
          ces.push_back(
              model.MakeCoedge(ring.top_e[i], Orientation::Forward));
        }
      }
      else
      {
        for (std::size_t i = 0; i < n; ++i)
      {
          const std::size_t k = n - 1 - i;
          ces.push_back(
              model.MakeCoedge(ring.top_e[k], Orientation::Reversed));
        }
      }
      Model::LinkLoop(loop, ces);
    }
  }

  // Side faces: outer walls face outward; hole walls face into the cavity.
  for (std::size_t ri = 0; ri < rings.size(); ++ri)
  {
    const RingGeom& ring = rings[ri];
    const std::size_t n = ring.bottom_v.size();
    for (std::size_t i = 0; i < n; ++i)
    {
      const std::size_t j = (i + 1) % n;
      if (!ring.is_hole)
      {
        const Point3d o = ring.bottom_v[i]->Position();
        const Vector3d u =
            (ring.bottom_v[j]->Position() - ring.bottom_v[i]->Position())
                .normalized();
        const Vector3d v = spec.Plane.Normal;
        add_quad_face(model, shell, ring.bottom_e[i], Orientation::Forward,
                      ring.vert_e[j], Orientation::Forward, ring.top_e[i],
                      Orientation::Reversed, ring.vert_e[i],
                      Orientation::Reversed, o, u, v,
                      spec.Name + "_side" + std::to_string(i));
      }
      else
      {
        const Point3d o = ring.bottom_v[j]->Position();
        const Vector3d u =
            (ring.bottom_v[i]->Position() - ring.bottom_v[j]->Position())
                .normalized();
        const Vector3d v = spec.Plane.Normal;
        add_quad_face(model, shell, ring.bottom_e[i], Orientation::Reversed,
                      ring.vert_e[i], Orientation::Forward, ring.top_e[i],
                      Orientation::Forward, ring.vert_e[j],
                      Orientation::Reversed, o, u, v,
                      spec.Name + "_h" + std::to_string(ri) + "_side" +
                          std::to_string(i));
      }
    }
  }

  // Pair radial partners: each edge should have 2 coedges.
  std::unordered_map<Edge*, std::vector<CoEdge*>> by_edge;
  for (Face* f : shell->Faces)
  {
    for (Loop* loop : f->Loops)
  {
      CoEdge* c = loop->First;
      if (!c) continue;
      do {
        by_edge[c->Edge].push_back(c);
        c = c->Next;
      } while (c && c != loop->First);
    }
  }
  for (auto& [e, ces] : by_edge)
  {
    (void)e;
    if (ces.size() == 2) Model::PairPartners(ces[0], ces[1]);
  }

  BREP_INFO("extrude_polygon '{}' n={} holes={} distance={}", spec.Name,
            poly.size(), rings.size() - 1, spec.Distance);
  return body;
}

}  // namespace

Profile2d ExtractProfile(const sketch::Sketch& sketch)
{
  Profile2d profile;
  if (sketch.Points().empty()) return profile;

  // If we have lines, walk them; else use all points as a polygon in order.
  if (!sketch.Lines().empty())
  {
    std::vector<bool> used(sketch.Lines().size(), false);
    sketch::SketchEntityId cur = sketch.Lines().front().P0;
    profile.Outer.push_back(sketch.Point(cur)->P);
    for (std::size_t guard = 0; guard < sketch.Lines().size(); ++guard)
    {
      bool advanced = false;
      for (std::size_t i = 0; i < sketch.Lines().size(); ++i)
      {
        if (used[i]) continue;
        const auto& ln = sketch.Lines()[i];
        if (ln.P0 == cur)
        {
          cur = ln.P1;
          used[i] = true;
          if (const auto* pt = sketch.Point(cur)) profile.Outer.push_back(pt->P);
          advanced = true;
          break;
        }
        if (ln.P1 == cur)
        {
          cur = ln.P0;
          used[i] = true;
          if (const auto* pt = sketch.Point(cur)) profile.Outer.push_back(pt->P);
          advanced = true;
          break;
        }
      }
      if (!advanced) break;
    }
    // Drop duplicate closing point.
    if (profile.Outer.size() >= 2 &&
        std::abs(profile.Outer.front().u() - profile.Outer.back().u()) < 1e-9 &&
        std::abs(profile.Outer.front().v() - profile.Outer.back().v()) < 1e-9)
        {
      profile.Outer.pop_back();
    }
  }
  else
        {
    for (const auto& p : sketch.Points()) profile.Outer.push_back(p.P);
  }
  return profile;
}

Body* Extrude(Model& model, const ExtrudeSpec& spec)
{
  if (spec.Profile.Outer.size() < 3 || std::abs(spec.Distance) < 1e-12)
{
    BREP_WARN("extrude: invalid profile or distance");
    return nullptr;
  }

  // Holes require prism path (box builder has no Inner loops).
  if (spec.Profile.Holes.empty())
  {
    double min_u = 0, max_u = 0, min_v = 0, max_v = 0;
    if (is_axis_aligned_rect(spec.Profile, min_u, max_u, min_v, max_v))
    {
      const Plane& pl = spec.Plane;
      const bool y_up =
          std::abs(pl.Normal.y() - 1.0) < 1e-6 &&
          std::abs(pl.UAxis.x() - 1.0) < 1e-6 &&
          std::abs(pl.VAxis.z() - 1.0) < 1e-6;
      if (y_up)
      {
        const double d0 = spec.Symmetric ? -0.5 * spec.Distance : 0.0;
        const double d1 = spec.Symmetric ? 0.5 * spec.Distance : spec.Distance;
        const double miny = std::min(d0, d1);
        const double maxy = std::max(d0, d1);
        BoxSpec box;
        box.Min = Point3d{min_u, miny, min_v};
        box.Max = Point3d{max_u, maxy, max_v};
        box.Name = spec.Name;
        box.Tolerance = spec.Tolerance;
        return MakeBox(model, box);
      }
    }
  }

  return extrude_polygon(model, spec);
}

}  // namespace brep::ops
