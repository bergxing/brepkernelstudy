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

/// Sample one loop into the parameter space of a plane or sphere.
[[nodiscard]] SampledRing sample_loop(const Loop& loop, const Surface& surface,
                                      const TessellationOptions& opts);

/// Sample one edge in its coedge direction using chord-height deflection.
[[nodiscard]] std::vector<Point3d> sample_edge_xyz(
    const CoEdge& ce, const TessellationOptions& opts);

}  // namespace brep::mesh
