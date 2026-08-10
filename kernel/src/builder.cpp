#include "brep/builder.hpp"

#include "brep/log.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace brep {
namespace {

constexpr int vid(int ix, int iy, int iz) noexcept {
  return ix | (iy << 1) | (iz << 2);
}

Point3d corner(const BoxSpec& s, int ix, int iy, int iz) {
  return {
      ix ? s.max.x() : s.min.x(),
      iy ? s.max.y() : s.min.y(),
      iz ? s.max.z() : s.min.z(),
  };
}

struct EdgeDef {
  int a;
  int b;
  const char* name;
};

constexpr EdgeDef kEdges[12] = {
    {vid(0, 0, 0), vid(1, 0, 0), "e00"},
    {vid(1, 0, 0), vid(1, 1, 0), "e01"},
    {vid(1, 1, 0), vid(0, 1, 0), "e02"},
    {vid(0, 1, 0), vid(0, 0, 0), "e03"},
    {vid(0, 0, 1), vid(1, 0, 1), "e10"},
    {vid(1, 0, 1), vid(1, 1, 1), "e11"},
    {vid(1, 1, 1), vid(0, 1, 1), "e12"},
    {vid(0, 1, 1), vid(0, 0, 1), "e13"},
    {vid(0, 0, 0), vid(0, 0, 1), "ez0"},
    {vid(1, 0, 0), vid(1, 0, 1), "ez1"},
    {vid(1, 1, 0), vid(1, 1, 1), "ez2"},
    {vid(0, 1, 0), vid(0, 1, 1), "ez3"},
};

struct FaceBuild {
  const char* name;
  Point3d origin;
  Vector3d u_axis;
  Vector3d v_axis;
  std::array<int, 4> edge_idx;
  std::array<bool, 4> forward;
  std::array<Point2d, 5> uv;
};

}  // namespace

Body* make_box(Model& model, const BoxSpec& spec) {
  if (!(spec.max.x() > spec.min.x() && spec.max.y() > spec.min.y() &&
        spec.max.z() > spec.min.z())) {
    BREP_ERROR("make_box: invalid extents min={} max={}", spec.min, spec.max);
    throw std::invalid_argument("make_box: max must be strictly greater than min");
  }

  BREP_INFO("make_box '{}' min={} max={} tol={:.3g}", spec.name, spec.min,
            spec.max, spec.tolerance);

  const double dx = spec.max.x() - spec.min.x();
  const double dy = spec.max.y() - spec.min.y();
  const double dz = spec.max.z() - spec.min.z();
  const double tol = spec.tolerance;

  std::array<Vertex*, 8> V{};
  for (int iz = 0; iz < 2; ++iz) {
    for (int iy = 0; iy < 2; ++iy) {
      for (int ix = 0; ix < 2; ++ix) {
        const int i = vid(ix, iy, iz);
        Point* p = model.make_point(corner(spec, ix, iy, iz), "p" + std::to_string(i));
        V[i] = model.make_vertex(p, tol, "v" + std::to_string(i));
      }
    }
  }

  std::array<Edge*, 12> E{};
  for (int i = 0; i < 12; ++i) {
    const EdgeDef& d = kEdges[i];
    LineCurve* curve = model.make_line(V[d.a]->position(), V[d.b]->position());
    E[i] = model.make_edge(curve, V[d.a], V[d.b], 0.0, curve->length(), tol, d.name);
  }

  const FaceBuild faces[6] = {
      // Loop must be CCW when viewed against the outward normal (-Z).
      {"f_zmin",
       {spec.min.x(), spec.min.y(), spec.min.z()},
       {1, 0, 0},
       {0, -1, 0},
       {3, 2, 1, 0},
       {false, false, false, false},
       {Point2d{0, 0}, Point2d{0, -dy}, Point2d{dx, -dy}, Point2d{dx, 0},
        Point2d{0, 0}}},
      {"f_zmax",
       {spec.min.x(), spec.min.y(), spec.max.z()},
       {1, 0, 0},
       {0, 1, 0},
       {4, 5, 6, 7},
       {true, true, true, true},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dy}, Point2d{0, dy},
        Point2d{0, 0}}},
      {"f_ymin",
       {spec.min.x(), spec.min.y(), spec.min.z()},
       {1, 0, 0},
       {0, 0, 1},
       {0, 9, 4, 8},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
      {"f_ymax",
       {spec.max.x(), spec.max.y(), spec.min.z()},
       {-1, 0, 0},
       {0, 0, 1},
       {2, 11, 6, 10},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
      {"f_xmin",
       {spec.min.x(), spec.min.y(), spec.min.z()},
       {0, 0, 1},
       {0, 1, 0},
       {8, 7, 11, 3},
       {true, false, false, true},
       {Point2d{0, 0}, Point2d{dz, 0}, Point2d{dz, dy}, Point2d{0, dy},
        Point2d{0, 0}}},
      {"f_xmax",
       {spec.max.x(), spec.min.y(), spec.min.z()},
       {0, 1, 0},
       {0, 0, 1},
       {1, 10, 5, 9},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dy, 0}, Point2d{dy, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
  };

  Body* body = model.make_body(BodyType::Solid, spec.name);
  Shell* shell = model.make_shell(true, spec.name + "_shell");
  body->shells.push_back(shell);

  std::array<std::vector<CoEdge*>, 12> by_edge{};

  for (const FaceBuild& fd : faces) {
    PlaneSurface* surf = model.make_plane(fd.origin, fd.u_axis, fd.v_axis, fd.name);
    Face* face = model.make_face(surf, Orientation::Forward, fd.name);
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, std::string(fd.name) + "_outer");

    std::array<CoEdge*, 4> ces{};
    for (int k = 0; k < 4; ++k) {
      const int ei = fd.edge_idx[k];
      const Orientation sense =
          fd.forward[k] ? Orientation::Forward : Orientation::Reversed;
      Curve2d* pc = model.make_line2d(fd.uv[k], fd.uv[k + 1]);
      ces[k] = model.make_coedge(
          E[ei], sense, pc, std::string(fd.name) + "_ce" + std::to_string(k));
      by_edge[static_cast<std::size_t>(ei)].push_back(ces[k]);
    }
    Model::link_loop(loop, ces);
  }

  for (int i = 0; i < 12; ++i) {
    if (by_edge[static_cast<std::size_t>(i)].size() != 2) {
      BREP_ERROR("make_box: edge[{}] radial degree={}", i,
                 by_edge[static_cast<std::size_t>(i)].size());
      throw std::runtime_error(
          "make_box: each edge must be shared by exactly two faces");
    }
    Model::pair_partners(by_edge[static_cast<std::size_t>(i)][0],
                         by_edge[static_cast<std::size_t>(i)][1]);
  }

  BREP_INFO("make_box '{}' done: 8 verts, 12 edges, 6 faces", spec.name);
  return body;
}

Body* make_sphere(Model& model, const SphereSpec& spec) {
  if (!(spec.radius > 0.0)) {
    BREP_ERROR("make_sphere: invalid radius={}", spec.radius);
    throw std::invalid_argument("make_sphere: radius must be positive");
  }

  const int slices = std::max(3, spec.slices);
  const int stacks = std::max(2, spec.stacks);
  const double r = spec.radius;
  const double tol = spec.tolerance;
  BREP_INFO("make_sphere '{}' center={} r={:.6g} slices={} stacks={}",
            spec.name, spec.center, r, slices, stacks);

  // Vertex grid: (stacks+1) rows × slices columns; poles share row verts.
  std::vector<Vertex*> verts;
  verts.reserve(static_cast<std::size_t>((stacks + 1) * slices));

  auto add_vertex = [&](Point3d p, const std::string& name) -> Vertex* {
    Point* pt = model.make_point(p, name);
    return model.make_vertex(pt, tol, name);
  };

  for (int i = 0; i <= stacks; ++i) {
    const double v = static_cast<double>(i) / static_cast<double>(stacks);
    const double phi = v * std::numbers::pi;  // 0..pi
    const double y = std::cos(phi);
    const double ring_r = std::sin(phi);
    for (int j = 0; j < slices; ++j) {
      if (i == 0 || i == stacks) {
        // Poles: one vertex per row (reuse first column).
        if (j == 0) {
          verts.push_back(add_vertex(
              Point3d{spec.center.x(), spec.center.y() + r * y,
                      spec.center.z()},
              "sv_pole_" + std::to_string(i)));
        } else {
          verts.push_back(verts[static_cast<std::size_t>(i * slices)]);
        }
        continue;
      }
      const double u = static_cast<double>(j) / static_cast<double>(slices);
      const double theta = u * 2.0 * std::numbers::pi;
      const double x = ring_r * std::cos(theta);
      const double z = ring_r * std::sin(theta);
      verts.push_back(add_vertex(
          Point3d{spec.center.x() + r * x, spec.center.y() + r * y,
                  spec.center.z() + r * z},
          "sv_" + std::to_string(i) + "_" + std::to_string(j)));
    }
  }

  auto vid = [&](int i, int j) -> std::size_t {
    return static_cast<std::size_t>(i * slices + (j % slices));
  };

  struct EdgeKey {
    Id a;
    Id b;
    bool operator==(const EdgeKey& o) const noexcept {
      return a == o.a && b == o.b;
    }
  };
  struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& k) const noexcept {
      return (static_cast<std::size_t>(k.a) * 1315423911u) ^
             static_cast<std::size_t>(k.b);
    }
  };

  std::unordered_map<EdgeKey, Edge*, EdgeKeyHash> edge_map;
  std::unordered_map<Edge*, std::vector<CoEdge*>> by_edge;

  auto get_or_make_edge = [&](Vertex* va, Vertex* vb) -> Edge* {
    const Id ia = va->id;
    const Id ib = vb->id;
    const EdgeKey key = ia < ib ? EdgeKey{ia, ib} : EdgeKey{ib, ia};
    if (auto it = edge_map.find(key); it != edge_map.end()) return it->second;
    Vertex* lo = ia < ib ? va : vb;
    Vertex* hi = ia < ib ? vb : va;
    LineCurve* curve = model.make_line(lo->position(), hi->position());
    Edge* e =
        model.make_edge(curve, lo, hi, 0.0, curve->length(), tol);
    edge_map.emplace(key, e);
    return e;
  };

  Body* body = model.make_body(BodyType::Solid, spec.name);
  Shell* shell = model.make_shell(true, spec.name + "_shell");
  body->shells.push_back(shell);

  auto add_tri = [&](Vertex* a, Vertex* b, Vertex* c, int face_i) {
    if (a == b || b == c || a == c) return;
    const Point3d pa = a->position();
    const Point3d pb = b->position();
    const Point3d pc = c->position();
    Vector3d n = (pb - pa).cross(pc - pa);
    if (n.norm() < 1e-14) return;
    n = n.normalized();
    // Outward = away from sphere center.
    if (n.dot(pa - spec.center) < 0.0) {
      std::swap(b, c);
      n = -n;
    }

    const std::string fname =
        spec.name + "_f" + std::to_string(face_i);
    PlaneSurface* surf =
        model.make_plane(pa, (b->position() - pa).normalized(),
                         (c->position() - pa).normalized(), fname);
    Face* face = model.make_face(surf, Orientation::Forward, fname);
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, fname + "_outer");

    Vertex* vv[3] = {a, b, c};
    std::array<CoEdge*, 3> ces{};
    for (int k = 0; k < 3; ++k) {
      Vertex* v0 = vv[k];
      Vertex* v1 = vv[(k + 1) % 3];
      Edge* e = get_or_make_edge(v0, v1);
      const bool forward = (e->v0 == v0);
      const Orientation sense =
          forward ? Orientation::Forward : Orientation::Reversed;
      Point2d uva, uvb;
      if (k == 0) {
        uva = Point2d{0, 0};
        uvb = Point2d{1, 0};
      } else if (k == 1) {
        uva = Point2d{1, 0};
        uvb = Point2d{0.5, 1};
      } else {
        uva = Point2d{0.5, 1};
        uvb = Point2d{0, 0};
      }
      Curve2d* pc = model.make_line2d(uva, uvb);
      ces[k] = model.make_coedge(e, sense, pc,
                                 fname + "_ce" + std::to_string(k));
      by_edge[e].push_back(ces[k]);
    }
    Model::link_loop(loop, ces);
  };

  int face_i = 0;
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      Vertex* v00 = verts[vid(i, j)];
      Vertex* v10 = verts[vid(i + 1, j)];
      Vertex* v01 = verts[vid(i, j + 1)];
      Vertex* v11 = verts[vid(i + 1, j + 1)];
      if (i == 0) {
        add_tri(v00, v10, v11, face_i++);
      } else if (i + 1 == stacks) {
        add_tri(v00, v10, v01, face_i++);
      } else {
        add_tri(v00, v10, v11, face_i++);
        add_tri(v00, v11, v01, face_i++);
      }
    }
  }

  for (auto& [e, ces] : by_edge) {
    if (ces.size() != 2) {
      BREP_ERROR("make_sphere: edge radial degree={}", ces.size());
      throw std::runtime_error(
          "make_sphere: each edge must be shared by exactly two faces");
    }
    Model::pair_partners(ces[0], ces[1]);
  }

  BREP_INFO("make_sphere '{}' done: {} faces", spec.name, face_i);
  return body;
}

}  // namespace brep
