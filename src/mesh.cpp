#include "brep/mesh.hpp"

#include "brep/log.hpp"

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
      const std::uint32_t base =
          static_cast<std::uint32_t>(mesh.vertices.size());
      for (const Point3d& p : ring) {
        mesh.vertices.push_back(MeshVertex{p, n});
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
