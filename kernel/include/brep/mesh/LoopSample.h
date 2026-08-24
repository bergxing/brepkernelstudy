#pragma once

#include "brep/Mesh.h"

#include <vector>

namespace brep::mesh
{

struct SampledPoint
{
  Point3d Xyz;
  Point2d Uv;
};

struct SampledRing
{
  LoopType Type{LoopType::Outer};
  std::vector<SampledPoint> Points;
};

struct FaceRegion
{
  SampledRing Outer;
  std::vector<SampledRing> Holes;
};

/// Sample one loop into the parameter space of a plane or sphere.
[[nodiscard]] SampledRing SampleLoop(const Loop& loop, const Surface& surface,
                                     const TessellationOptions& opts);

/// Sample face loops and assign each inner ring to its containing outer ring.
[[nodiscard]] std::vector<FaceRegion> GroupFaceRegions(
    const Face& face, const Surface& surface, const TessellationOptions& opts);

/// Make ring UV contiguous: if |Δu|>π between adjacent samples, shift by ±2π
/// so the polyline does not jump the seam. May expand u outside [0,2π).
[[nodiscard]] SampledRing UnwrapSphereRing(SampledRing ring);

/// Sample one edge in its coedge direction using chord-height deflection.
[[nodiscard]] std::vector<Point3d> SampleEdgeXyz(
    const CoEdge& ce, const TessellationOptions& opts);

}  // namespace brep::mesh
