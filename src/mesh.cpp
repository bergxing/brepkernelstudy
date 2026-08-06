#include "brep/mesh.hpp"

#include "brep/geometry.hpp"
#include "brep/log.hpp"

#include <algorithm>
#include <limits>
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

}  // namespace

TriangleMesh tessellate_body(const Body& body) {
  TriangleMesh mesh;

  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (const Face* face : shell->faces) {
      if (!face || !face->surface) continue;
      const Loop* loop = face->outer_loop();
      if (!loop || !loop->first) continue;

      std::vector<Point3d> ring;
      loop->for_each_coedge([&](const CoEdge& ce) {
        if (Vertex* v = ce.from()) {
          ring.push_back(v->position());
        }
      });
      if (ring.size() < 3) {
        BREP_WARN("tessellate: face '{}' has <3 loop vertices", face->name);
        continue;
      }

      const Vector3d n = face->normal_at(0.0, 0.0);
      std::vector<Point2d> raw_uv(ring.size());
      double u_min = std::numeric_limits<double>::infinity();
      double u_max = -std::numeric_limits<double>::infinity();
      double v_min = std::numeric_limits<double>::infinity();
      double v_max = -std::numeric_limits<double>::infinity();

      if (const auto* plane = dynamic_cast<const PlaneSurface*>(face->surface)) {
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

      const std::uint32_t base =
          static_cast<std::uint32_t>(mesh.vertices.size());
      for (std::size_t i = 0; i < ring.size(); ++i) {
        MeshVertex mv;
        mv.position = ring[i];
        mv.normal = n;
        mv.uv = Point2d{(raw_uv[i].u() - u_min) / du,
                        (raw_uv[i].v() - v_min) / dv};
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
  }

  BREP_INFO("tessellate_body '{}': {} verts, {} tris", body.name,
            mesh.vertices.size(), mesh.indices.size() / 3);
  return mesh;
}

EdgeMesh extract_edges(const Body& body) {
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
