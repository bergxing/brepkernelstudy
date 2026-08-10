#include "brep/mesh.hpp"

#include "brep/geometry.hpp"
#include "brep/log.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_set>
#include <utility>

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

void tessellate_plane_face(const Face& face, TriangleMesh& mesh) {
  const Loop* loop = face.outer_loop();
  if (!loop || !loop->first) return;

  std::vector<Point3d> ring;
  loop->for_each_coedge([&](const CoEdge& ce) {
    if (Vertex* v = ce.from()) {
      ring.push_back(v->position());
    }
  });
  if (ring.size() < 3) {
    BREP_WARN("tessellate: face '{}' has <3 loop vertices", face.name);
    return;
  }

  const Vector3d n = face.normal_at(0.0, 0.0);

  // Ensure ring winding matches the outward face normal so Vulkan
  // back-face culling keeps exterior faces visible.
  if (ring.size() >= 3) {
    const Vector3d geom_n = (ring[1] - ring[0]).cross(ring[2] - ring[0]);
    if (geom_n.dot(n) < 0.0) {
      std::reverse(ring.begin(), ring.end());
    }
  }

  std::vector<Point2d> raw_uv(ring.size());
  double u_min = std::numeric_limits<double>::infinity();
  double u_max = -std::numeric_limits<double>::infinity();
  double v_min = std::numeric_limits<double>::infinity();
  double v_max = -std::numeric_limits<double>::infinity();

  if (const auto* plane = dynamic_cast<const PlaneSurface*>(face.surface)) {
    for (std::size_t i = 0; i < ring.size(); ++i) {
      raw_uv[i] = plane->param_of(ring[i]);
      u_min = std::min(u_min, raw_uv[i].u());
      u_max = std::max(u_max, raw_uv[i].u());
      v_min = std::min(v_min, raw_uv[i].v());
      v_max = std::max(v_max, raw_uv[i].v());
    }
  } else {
    for (std::size_t i = 0; i < ring.size(); ++i) {
      raw_uv[i] = Point2d{0.0, 0.0};
    }
    u_min = 0.0;
    u_max = 1.0;
    v_min = 0.0;
    v_max = 1.0;
  }

  const double du = std::max(u_max - u_min, 1e-9);
  const double dv = std::max(v_max - v_min, 1e-9);

  const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
  for (std::size_t i = 0; i < ring.size(); ++i) {
    MeshVertex mv;
    mv.position = ring[i];
    mv.normal = n;
    mv.uv = Point2d{(raw_uv[i].u() - u_min) / du, (raw_uv[i].v() - v_min) / dv};
    mesh.vertices.push_back(mv);
  }

  // Fan triangulation from vertex 0 (valid for convex loops; box faces are).
  for (std::uint32_t i = 1; i + 1 < static_cast<std::uint32_t>(ring.size());
       ++i) {
    mesh.indices.push_back(base);
    mesh.indices.push_back(base + i);
    mesh.indices.push_back(base + i + 1);
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
