#pragma once

#include "brep/bool/Context.h"
#include "brep/spatial/FaceBvh.h"
#include "brep/Topology.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace brep::boolean
{

/// Face–face broad-phase for the general boolean path (T4.4.c).
///
/// BuildQuality choice:
/// - `Sah` (preferred default here): binning SAH tends to cut empty space better
///   when bodies have uneven face AABB sizes; use for production probes.
/// - `Median`: cheaper build, same query API; useful for A/B and tiny models.
/// Query/traversal code is shared; only the split heuristic differs (T4.4.a/b).

struct FacePairCandidate
{
  Face* a{nullptr};
  Face* b{nullptr};
};

struct BroadphaseProbe
{
  spatial::BuildQuality Quality{spatial::BuildQuality::Sah};
  std::size_t CandidatePairs{0};
  std::size_t AttemptedIntersects{0};
  std::size_t NonemptyIntersects{0};
  std::size_t UnsupportedPairs{0};
  std::string Summary;
};

[[nodiscard]] std::vector<FacePairCandidate> CollectFacePairCandidates(
    const Body& a, const Body& b,
    spatial::BuildQuality quality = spatial::BuildQuality::Sah);

/// Build FaceBvh candidates then dispatch known analytic `intersect_*` pairs.
/// Does not imprint or classify — smoke / diagnostic helper for T4.4.c.
[[nodiscard]] BroadphaseProbe ProbeFacePairIntersections(
    const Body& a, const Body& b,
    spatial::BuildQuality quality = spatial::BuildQuality::Sah,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
