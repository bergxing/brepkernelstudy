#pragma once

#include "brep/mesh.hpp"

#include <vector>

namespace brep::mesh {

struct SampledPoint {
  Point3d xyz;
  Point2d uv;
};

struct SampledRing {
  LoopType type{LoopType::Outer};
  std::vector<SampledPoint> points;
};

struct FaceRegion {
  SampledRing outer;
  std::vector<SampledRing> holes;
};

/// Sample one loop into the parameter space of a plane or sphere.
[[nodiscard]] SampledRing sample_loop(const Loop& loop, const Surface& surface,
                                      const TessellationOptions& opts);

/// Sample face loops and assign each inner ring to its containing outer ring.
[[nodiscard]] std::vector<FaceRegion> group_face_regions(
    const Face& face, const Surface& surface, const TessellationOptions& opts);

/// Make ring UV contiguous: if |Δu|>π between adjacent samples, shift by ±2π
/// so the polyline does not jump the seam. May expand u outside [0,2π).
[[nodiscard]] SampledRing unwrap_sphere_ring(SampledRing ring);

/// Sample one edge in its coedge direction using chord-height deflection.
[[nodiscard]] std::vector<Point3d> sample_edge_xyz(
    const CoEdge& ce, const TessellationOptions& opts);

}  // namespace brep::mesh
