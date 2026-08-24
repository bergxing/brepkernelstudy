#pragma once

#include "brep/Math.h"
#include "brep/Topology.h"

#include <cstdint>
#include <numbers>
#include <vector>

namespace brep
{

struct MeshVertex
{
  Point3d Position;
  Vector3d Normal;
  Point2d Uv{};
};

struct TriangleMesh
{
  std::vector<MeshVertex> Vertices;
  std::vector<std::uint32_t> Indices;
};

/// Segment list: positions are stored as pairs (a0,b0, a1,b1, ...).
struct EdgeMesh
{
  std::vector<Point3d> Positions;
};

/// Display tessellation controls (see design §A3).
struct TessellationOptions
{
  /// Chord-height budget. If ≤ 0, sphere faces use max(0.02·R, 1e-4).
  double LinearDeflection{0.0};
  /// Max adjacent normal angle (radians). Default 15°.
  double AngularDeflection{15.0 * std::numbers::pi / 180.0};
  int MinUSegments{24};
  int MinVSegments{12};
  int MaxUSegments{128};
  int MaxVSegments{64};

  [[nodiscard]] static TessellationOptions ForRadius(double radius);
};

/// Tessellate one face into `out` (appends vertices/indices).
void TessellateFace(const Face& face, TriangleMesh& out,
                     const TessellationOptions& opts = {});

/// Tessellate all faces. Plane keeps fan triangulation; Sphere uses UV grid
/// with analytic normals. Generates per-face UVs in [0,1].
[[nodiscard]] TriangleMesh TessellateBody(
    const Body& body, const TessellationOptions& opts = {});

/// Edge display extraction (see design §A4).
struct EdgeExtractionOptions
{
  /// Periodic seams (both coedges on the same face) are hidden by default.
  bool IncludeSeamEdges{false};
};

/// Extract unique topological edges as line segments.
[[nodiscard]] EdgeMesh ExtractEdges(const Body& body,
                                   const EdgeExtractionOptions& opts = {});

}  // namespace brep
