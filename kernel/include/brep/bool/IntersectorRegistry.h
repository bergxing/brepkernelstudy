#pragma once

#include "brep/bool/Broadphase.h"
#include "brep/bool/Context.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/Geometry.h"

#include <cstdint>
#include <vector>

namespace brep::boolean
{

enum class IntersectProbe : std::uint8_t
{
  Nonempty = 0,
  Empty,
  Unsupported,
};

/// Probe one face pair with the analytic Intersect* kernels.
[[nodiscard]] IntersectProbe ProbeAnalyticSurfacePair(
    const Face& a, const Face& b, const BooleanContext& ctx = {});

/// Dispatch analytic surface–surface intersection by SurfaceKind pair.
class IntersectorRegistry
{
 public:
  IntersectorRegistry();

  [[nodiscard]] IntersectProbe Probe(const Face& a, const Face& b,
                                     const BooleanContext& ctx) const;

  /// Geometric intersection segments for a face pair (M1.1).
  [[nodiscard]] std::vector<IntersectionSegment> Intersect(
      const Face& a, const Face& b, const BooleanContext& ctx) const;

  [[nodiscard]] IntersectionGraph BuildGraph(
      const Body& bodyA, const Body& bodyB, const BooleanContext& ctx = {},
      spatial::BuildQuality quality = spatial::BuildQuality::Sah) const;

  [[nodiscard]] BroadphaseProbe ProbeBodyPair(
      const Body& a, const Body& b,
      spatial::BuildQuality quality = spatial::BuildQuality::Sah,
      const BooleanContext& ctx = {}) const;
};

}  // namespace brep::boolean
