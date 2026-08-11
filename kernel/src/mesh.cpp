#include "brep/mesh.hpp"

#include "brep/geometry.hpp"
#include "brep/log.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_set>
#include <utility>
#include <vector>

namespace brep {
namespace {

struct EdgeKey {
  Id a;
  Id b;
  bool operator==(const EdgeKey& o) const noexcept { return a == o.a && b == o.b; }
};

struct EdgeKeyHash {
  std::size_t operator()(const EdgeKey& k) const noexcept {
    return (static_cast<std::size_t>(k.a) * 1315423911u) ^
           static_cast<std::size_t>(k.b);
  }
};

EdgeKey make_edge_key(const Edge& e) {
  const Id i0 = e.v0 ? e.v0->id : 0;
  const Id i1 = e.v1 ? e.v1->id : 0;
  return i0 < i1 ? EdgeKey{i0, i1} : EdgeKey{i1, i0};
}

/// Periodic seam: both radial coedges belong to the same face (e.g. sphere
/// meridian). Ordinary manifold edges have coedges on two different faces.
bool is_periodic_seam_edge(const Edge& e) {
  if (e.radial.size() != 2) return false;
  const CoEdge* a = e.radial[0];
  const CoEdge* b = e.radial[1];
  if (!a || !b || !a->loop || !b->loop) return false;
  return a->loop->face != nullptr && a->loop->face == b->loop->face;
}

int clamp_segments(int value, int lo, int hi) {
  return std::clamp(value, std::min(lo, hi), std::max(lo, hi));
}

[[nodiscard]] double ring_signed_area2d(const std::vector<Point2d>& ring) {
  double a = 0.0;
  const std::size_t n = ring.size();
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t j = (i + 1) % n;
    a += ring[i].u() * ring[j].v() - ring[j].u() * ring[i].v();
  }
  return 0.5 * a;
}

void ensure_ccw(std::vector<Point2d>& uv, std::vector<Point3d>& xyz) {
  if (ring_signed_area2d(uv) < 0.0) {
    std::reverse(uv.begin(), uv.end());
    std::reverse(xyz.begin(), xyz.end());
  }
}

void ensure_cw(std::vector<Point2d>& uv, std::vector<Point3d>& xyz) {
  if (ring_signed_area2d(uv) > 0.0) {
    std::reverse(uv.begin(), uv.end());
    std::reverse(xyz.begin(), xyz.end());
  }
}

[[nodiscard]] double dist2_uv(const Point2d& a, const Point2d& b) {
  const double du = a.u() - b.u();
  const double dv = a.v() - b.v();
  return du * du + dv * dv;
}

[[nodiscard]] bool point_in_triangle2d(const Point2d& p, const Point2d& a,
                                       const Point2d& b, const Point2d& c) {
  const double area = (b.u() - a.u()) * (c.v() - a.v()) -
                      (b.v() - a.v()) * (c.u() - a.u());
  if (std::abs(area) < 1e-18) return false;
  const double s = ((a.u() - c.u()) * (p.v() - c.v()) -
                    (a.v() - c.v()) * (p.u() - c.u())) /
                   area;
  const double t = ((b.u() - a.u()) * (p.v() - a.v()) -
                    (b.v() - a.v()) * (p.u() - a.u())) /
                   area;
  return s >= -1e-12 && t >= -1e-12 && (s + t) <= 1.0 + 1e-12;
}

void collect_loop_ring(const Loop& loop, const PlaneSurface* plane,
                       std::vector<Point3d>& xyz, std::vector<Point2d>& uv) {
  xyz.clear();
  uv.clear();
  loop.for_each_coedge([&](const CoEdge& ce) {
    if (Vertex* v = ce.from()) {
      xyz.push_back(v->position());
      if (plane) {
        uv.push_back(plane->param_of(v->position()));
      } else {
        uv.push_back(Point2d{0.0, 0.0});
      }
    }
  });
}

/// Insert a CW hole into a CCW outer via a bridge (duplicated endpoints).
void bridge_hole(std::vector<Point2d>& outer_uv, std::vector<Point3d>& outer_xyz,
                 const std::vector<Point2d>& hole_uv,
                 const std::vector<Point3d>& hole_xyz) {
  if (hole_uv.size() < 3 || hole_uv.size() != hole_xyz.size()) return;

  std::size_t hr = 0;
  for (std::size_t i = 1; i < hole_uv.size(); ++i) {
    if (hole_uv[i].u() > hole_uv[hr].u() ||
        (hole_uv[i].u() == hole_uv[hr].u() &&
         hole_uv[i].v() > hole_uv[hr].v())) {
      hr = i;
    }
  }

  std::size_t br = 0;
  double best = dist2_uv(outer_uv[0], hole_uv[hr]);
  for (std::size_t i = 1; i < outer_uv.size(); ++i) {
    const double d = dist2_uv(outer_uv[i], hole_uv[hr]);
    if (d < best) {
      best = d;
      br = i;
    }
  }

  std::vector<Point2d> nu;
  std::vector<Point3d> nx;
  nu.reserve(outer_uv.size() + hole_uv.size() + 2);
  nx.reserve(outer_xyz.size() + hole_xyz.size() + 2);
  for (std::size_t i = 0; i <= br; ++i) {
    nu.push_back(outer_uv[i]);
    nx.push_back(outer_xyz[i]);
  }
  for (std::size_t k = 0; k < hole_uv.size(); ++k) {
    const std::size_t idx = (hr + k) % hole_uv.size();
    nu.push_back(hole_uv[idx]);
    nx.push_back(hole_xyz[idx]);
  }
  nu.push_back(hole_uv[hr]);
  nx.push_back(hole_xyz[hr]);
  nu.push_back(outer_uv[br]);
  nx.push_back(outer_xyz[br]);
  for (std::size_t i = br + 1; i < outer_uv.size(); ++i) {
    nu.push_back(outer_uv[i]);
    nx.push_back(outer_xyz[i]);
  }
  outer_uv.swap(nu);
  outer_xyz.swap(nx);
}

[[nodiscard]] bool is_convex_ear(const std::vector<Point2d>& poly, std::size_t i) {
  const std::size_t n = poly.size();
  const std::size_t i0 = (i + n - 1) % n;
  const std::size_t i1 = i;
  const std::size_t i2 = (i + 1) % n;
  const Point2d& a = poly[i0];
  const Point2d& b = poly[i1];
  const Point2d& c = poly[i2];
  // Interior angle convex for CCW polygon: cross(b-a, c-b) > 0
  const double cross =
      (b.u() - a.u()) * (c.v() - b.v()) - (b.v() - a.v()) * (c.u() - b.u());
  if (cross <= 1e-14) return false;
  for (std::size_t j = 0; j < n; ++j) {
    if (j == i0 || j == i1 || j == i2) continue;
    // Bridge insertion duplicates endpoints; ignore coincident verts.
    if (dist2_uv(poly[j], a) < 1e-20 || dist2_uv(poly[j], b) < 1e-20 ||
        dist2_uv(poly[j], c) < 1e-20) {
      continue;
    }
    if (point_in_triangle2d(poly[j], a, b, c)) return false;
  }
  return true;
}

void ear_clip_triangulate(const std::vector<Point2d>& uv,
                          std::vector<std::array<std::uint32_t, 3>>& tris) {
  const std::size_t n0 = uv.size();
  if (n0 < 3) return;
  std::vector<std::uint32_t> idx(n0);
  for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(n0); ++i) {
    idx[i] = i;
  }
  std::vector<Point2d> poly = uv;

  auto refresh_poly = [&]() {
    poly.resize(idx.size());
    for (std::size_t i = 0; i < idx.size(); ++i) poly[i] = uv[idx[i]];
  };

  int guard = static_cast<int>(n0) * static_cast<int>(n0) + 8;
  while (idx.size() > 3 && guard-- > 0) {
    bool clipped = false;
    for (std::size_t i = 0; i < idx.size(); ++i) {
      if (!is_convex_ear(poly, i)) continue;
      const std::size_t i0 = (i + idx.size() - 1) % idx.size();
      const std::size_t i2 = (i + 1) % idx.size();
      tris.push_back({idx[i0], idx[i], idx[i2]});
      idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
      refresh_poly();
      clipped = true;
      break;
    }
    if (!clipped) break;
  }
  if (idx.size() == 3) {
    tris.push_back({idx[0], idx[1], idx[2]});
  }
}

void tessellate_plane_face(const Face& face, TriangleMesh& mesh) {
  const Loop* outer = face.outer_loop();
  if (!outer || !outer->first) return;

  const auto* plane = dynamic_cast<const PlaneSurface*>(face.surface);
  std::vector<Point3d> outer_xyz;
  std::vector<Point2d> outer_uv;
  collect_loop_ring(*outer, plane, outer_xyz, outer_uv);
  if (outer_xyz.size() < 3) {
    BREP_WARN("tessellate: face '{}' has <3 outer vertices", face.name);
    return;
  }

  ensure_ccw(outer_uv, outer_xyz);

  for (Loop* loop : face.loops) {
    if (!loop || loop->type != LoopType::Inner) continue;
    std::vector<Point3d> hole_xyz;
    std::vector<Point2d> hole_uv;
    collect_loop_ring(*loop, plane, hole_xyz, hole_uv);
    if (hole_xyz.size() < 3) continue;
    ensure_cw(hole_uv, hole_xyz);
    bridge_hole(outer_uv, outer_xyz, hole_uv, hole_xyz);
  }

  const Vector3d n = face.normal_at(0.0, 0.0);

  // Keep UV CCW for ear clipping; flip triangle winding if 3D disagrees.
  bool flip_tris = false;
  if (outer_xyz.size() >= 3) {
    for (std::size_t i = 0; i < outer_xyz.size(); ++i) {
      const Point3d& a = outer_xyz[i];
      const Point3d& b = outer_xyz[(i + 1) % outer_xyz.size()];
      const Point3d& c = outer_xyz[(i + 2) % outer_xyz.size()];
      const Vector3d geom_n = (b - a).cross(c - a);
      if (geom_n.squaredNorm() < 1e-24) continue;
      flip_tris = geom_n.dot(n) < 0.0;
      break;
    }
  }

  double u_min = std::numeric_limits<double>::infinity();
  double u_max = -std::numeric_limits<double>::infinity();
  double v_min = std::numeric_limits<double>::infinity();
  double v_max = -std::numeric_limits<double>::infinity();
  for (const Point2d& p : outer_uv) {
    u_min = std::min(u_min, p.u());
    u_max = std::max(u_max, p.u());
    v_min = std::min(v_min, p.v());
    v_max = std::max(v_max, p.v());
  }
  if (!plane) {
    u_min = 0.0;
    u_max = 1.0;
    v_min = 0.0;
    v_max = 1.0;
  }
  const double du = std::max(u_max - u_min, 1e-9);
  const double dv = std::max(v_max - v_min, 1e-9);

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (std::size_t i = 0; i < outer_xyz.size(); ++i) {
    MeshVertex mv;
    mv.position = outer_xyz[i];
    mv.normal = n;
    mv.uv = Point2d{(outer_uv[i].u() - u_min) / du,
                    (outer_uv[i].v() - v_min) / dv};
    mesh.vertices.push_back(mv);
  }

  std::vector<std::array<std::uint32_t, 3>> tris;
  ear_clip_triangulate(outer_uv, tris);
  for (const auto& t : tris) {
    if (!flip_tris) {
      mesh.indices.push_back(base + t[0]);
      mesh.indices.push_back(base + t[1]);
      mesh.indices.push_back(base + t[2]);
    } else {
      mesh.indices.push_back(base + t[0]);
      mesh.indices.push_back(base + t[2]);
      mesh.indices.push_back(base + t[1]);
    }
  }
}

std::pair<int, int> sphere_segment_counts(double radius,
                                          const TessellationOptions& opts) {
  const double R = std::max(radius, 1e-12);
  const double h =
      opts.linear_deflection > 0.0
          ? opts.linear_deflection
          : std::max(0.02 * R, 1e-4);
  const double ang = std::max(opts.angular_deflection, 1e-6);

  int nu = static_cast<int>(std::ceil(2.0 * std::numbers::pi / ang));

  // Great-circle chord height h = R (1 - cos(α/2)) ⇒ α = 2 acos(1 - h/R).
  const double ratio = std::clamp(1.0 - h / R, -1.0, 1.0);
  const double alpha = 2.0 * std::acos(ratio);
  int nv_from_linear =
      alpha > 1e-12
          ? static_cast<int>(std::ceil(std::numbers::pi / alpha))
          : opts.max_v_segments;
  int nv_from_angular =
      static_cast<int>(std::ceil(std::numbers::pi / ang));
  int nv = std::max(nv_from_linear, nv_from_angular);

  nu = clamp_segments(nu, opts.min_u_segments, opts.max_u_segments);
  nv = clamp_segments(nv, opts.min_v_segments, opts.max_v_segments);
  return {nu, nv};
}

void tessellate_sphere_face(const Face& face, TriangleMesh& mesh,
                            const TessellationOptions& opts) {
  const auto* sphere = dynamic_cast<const SphereSurface*>(face.surface);
  if (!sphere) return;

  const auto [nu, nv] = sphere_segment_counts(sphere->radius(), opts);
  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  const int cols = nu + 1;

  for (int iv = 0; iv <= nv; ++iv) {
    const double v =
        -0.5 * std::numbers::pi +
        (static_cast<double>(iv) / static_cast<double>(nv)) * std::numbers::pi;
    for (int iu = 0; iu <= nu; ++iu) {
      // iu==nu duplicates u=0 at u=2π so the seam column shares positions.
      const double u =
          (static_cast<double>(iu) / static_cast<double>(nu)) *
          2.0 * std::numbers::pi;
      const double u_eval = (iu == nu) ? 0.0 : u;
      MeshVertex mv;
      mv.position = sphere->eval(u_eval, v);
      mv.normal = face.normal_at(u_eval, v);
      mv.uv = Point2d{static_cast<double>(iu) / static_cast<double>(nu),
                      static_cast<double>(iv) / static_cast<double>(nv)};
      mesh.vertices.push_back(mv);
    }
  }

  auto idx = [&](int iu, int iv) -> std::uint32_t {
    return base + static_cast<std::uint32_t>(iv * cols + iu);
  };

  // Probe a mid-latitude quad (pole rows are degenerate).
  const int probe_v = std::max(1, nv / 2);
  const std::uint32_t i00 = idx(0, probe_v);
  const std::uint32_t i10 = idx(1, probe_v);
  const std::uint32_t i01 = idx(0, probe_v + 1);
  const Point3d& p00 = mesh.vertices[i00].position;
  const Point3d& p10 = mesh.vertices[i10].position;
  const Point3d& p01 = mesh.vertices[i01].position;
  const Vector3d geom = (p10 - p00).cross(p01 - p00);
  const bool flip = geom.dot(mesh.vertices[i00].normal) < 0.0;

  auto push_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const Vector3d area =
        (mesh.vertices[b].position - mesh.vertices[a].position)
            .cross(mesh.vertices[c].position - mesh.vertices[a].position);
    if (area.squaredNorm() < 1e-24) return;
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
  };

  for (int iv = 0; iv < nv; ++iv) {
    for (int iu = 0; iu < nu; ++iu) {
      const std::uint32_t a = idx(iu, iv);
      const std::uint32_t b = idx(iu + 1, iv);
      const std::uint32_t c = idx(iu + 1, iv + 1);
      const std::uint32_t d = idx(iu, iv + 1);
      if (!flip) {
        push_tri(a, b, c);
        push_tri(a, c, d);
      } else {
        push_tri(a, d, c);
        push_tri(a, c, b);
      }
    }
  }
}

}  // namespace

TessellationOptions TessellationOptions::for_radius(double radius) {
  TessellationOptions opts;
  opts.linear_deflection = std::max(0.02 * std::abs(radius), 1e-4);
  return opts;
}

void tessellate_face(const Face& face, TriangleMesh& out,
                     const TessellationOptions& opts) {
  if (!face.surface) return;
  switch (face.surface->kind()) {
    case SurfaceKind::Sphere:
      tessellate_sphere_face(face, out, opts);
      break;
    case SurfaceKind::Plane:
      tessellate_plane_face(face, out);
      break;
    default:
      BREP_WARN("tessellate_face: unsupported surface kind on '{}'", face.name);
      break;
  }
}

TriangleMesh tessellate_body(const Body& body, const TessellationOptions& opts) {
  TriangleMesh mesh;

  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (const Face* face : shell->faces) {
      if (!face) continue;
      tessellate_face(*face, mesh, opts);
    }
  }

  BREP_INFO("tessellate_body '{}': {} verts, {} tris", body.name,
            mesh.vertices.size(), mesh.indices.size() / 3);
  return mesh;
}

EdgeMesh extract_edges(const Body& body, const EdgeExtractionOptions& opts) {
  EdgeMesh mesh;
  std::unordered_set<EdgeKey, EdgeKeyHash> seen;

  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (const Face* face : shell->faces) {
      if (!face) continue;
      for (const Loop* loop : face->loops) {
        if (!loop) continue;
        loop->for_each_coedge([&](const CoEdge& ce) {
          if (!ce.edge || !ce.edge->v0 || !ce.edge->v1) return;
          if (!opts.include_seam_edges && is_periodic_seam_edge(*ce.edge)) {
            return;
          }
          const EdgeKey key = make_edge_key(*ce.edge);
          if (!seen.insert(key).second) return;
          mesh.positions.push_back(ce.edge->v0->position());
          mesh.positions.push_back(ce.edge->v1->position());
        });
      }
    }
  }

  BREP_INFO("extract_edges '{}': {} segments", body.name,
            mesh.positions.size() / 2);
  return mesh;
}

}  // namespace brep
