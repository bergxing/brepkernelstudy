#pragma once

#include "brep/bool/context.hpp"
#include "brep/spatial/face_bvh.hpp"
#include "brep/topology.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace brep::boolean {

/// Face–face broad-phase for the general boolean path (T4.4.c).
///
/// BuildQuality choice:
/// - `Sah` (preferred default here): binning SAH tends to cut empty space better
///   when bodies have uneven face AABB sizes; use for production probes.
/// - `Median`: cheaper build, same query API; useful for A/B and tiny models.
/// Query/traversal code is shared; only the split heuristic differs (T4.4.a/b).

struct FacePairCandidate {
  Face* a{nullptr};
  Face* b{nullptr};
};

struct BroadphaseProbe {
  spatial::BuildQuality quality{spatial::BuildQuality::Sah};
  std::size_t candidate_pairs{0};
  std::size_t attempted_intersects{0};
  std::size_t nonempty_intersects{0};
  std::size_t unsupported_pairs{0};
  std::string summary;
};

[[nodiscard]] std::vector<FacePairCandidate> collect_face_pair_candidates(
    const Body& a, const Body& b,
    spatial::BuildQuality quality = spatial::BuildQuality::Sah);

/// Build FaceBvh candidates then dispatch known analytic `intersect_*` pairs.
/// Does not imprint or classify — smoke / diagnostic helper for T4.4.c.
[[nodiscard]] BroadphaseProbe probe_face_pair_intersections(
    const Body& a, const Body& b,
    spatial::BuildQuality quality = spatial::BuildQuality::Sah,
    const BooleanContext& ctx = {});

}  // namespace brep::boolean
