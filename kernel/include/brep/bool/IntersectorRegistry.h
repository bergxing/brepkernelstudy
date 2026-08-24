#pragma once

#include "brep/bool/Broadphase.h"
#include "brep/bool/Context.h"
#include "brep/Geometry.h"

namespace brep::boolean
{

enum class IntersectProbe : std::uint8_t
{
  Nonempty = 0,
  Empty,
  Unsupported,
};

/// Dispatch analytic surface–surface intersection by SurfaceKind pair.
class IntersectorRegistry
{
 public:
  IntersectorRegistry();

  [[nodiscard]] IntersectProbe Probe(const Face& a, const Face& b,
                                     const BooleanContext& ctx) const;

  [[nodiscard]] BroadphaseProbe ProbeBodyPair(
      const Body& a, const Body& b,
      spatial::BuildQuality quality = spatial::BuildQuality::Sah,
      const BooleanContext& ctx = {}) const;
};

}  // namespace brep::boolean
