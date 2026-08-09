#pragma once

#include "brep/snap/snap_types.hpp"

#include <span>
#include <vector>

namespace brep {

class Body;

std::vector<SnapCandidate> query_snap_candidates(
    std::span<Body* const> bodies, const SnapQuery& query);

}  // namespace brep
