#pragma once

#include "brep/math.hpp"
#include "brep/topology.hpp"

#include <cstdint>
#include <vector>

namespace brep {

struct MeshVertex {
  Point3d position;
  Vector3d normal;
};

struct TriangleMesh {
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;
};

/// Segment list: positions are stored as pairs (a0,b0, a1,b1, ...).
struct EdgeMesh {
  std::vector<Point3d> positions;
};

/// Tessellate planar faces (current kernel: PlaneSurface + polygonal outer loop).
[[nodiscard]] TriangleMesh tessellate_body(const Body& body);

/// Extract unique topological edges as line segments.
[[nodiscard]] EdgeMesh extract_edges(const Body& body);

}  // namespace brep
