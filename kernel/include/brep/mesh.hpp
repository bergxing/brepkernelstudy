#pragma once

#include "brep/math.hpp"
#include "brep/topology.hpp"

#include <cstdint>
#include <numbers>
#include <vector>

namespace brep {

struct MeshVertex {
  Point3d position;
  Vector3d normal;
  Point2d uv{};
};

struct TriangleMesh {
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;
};

/// Segment list: positions are stored as pairs (a0,b0, a1,b1, ...).
struct EdgeMesh {
  std::vector<Point3d> positions;
};

/// Display tessellation controls (see design §A3).
struct TessellationOptions {
  /// Chord-height budget. If ≤ 0, sphere faces use max(0.02·R, 1e-4).
  double linear_deflection{0.0};
  /// Max adjacent normal angle (radians). Default 15°.
  double angular_deflection{15.0 * std::numbers::pi / 180.0};
  int min_u_segments{24};
  int min_v_segments{12};
  int max_u_segments{128};
  int max_v_segments{64};

  [[nodiscard]] static TessellationOptions for_radius(double radius);
};

/// Tessellate one face into `out` (appends vertices/indices).
void tessellate_face(const Face& face, TriangleMesh& out,
                     const TessellationOptions& opts = {});

/// Tessellate all faces. Plane keeps fan triangulation; Sphere uses UV grid
/// with analytic normals. Generates per-face UVs in [0,1].
[[nodiscard]] TriangleMesh tessellate_body(
    const Body& body, const TessellationOptions& opts = {});

/// Extract unique topological edges as line segments.
[[nodiscard]] EdgeMesh extract_edges(const Body& body);

}  // namespace brep
