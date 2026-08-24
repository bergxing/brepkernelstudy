#pragma once

#include "brep/snap/SnapTypes.h"

#include <span>
#include <vector>

namespace brep
{

class Body;

std::vector<SnapCandidate> QuerySnapCandidates(
    std::span<Body* const> bodies, const SnapQuery& query);

}  // namespace brep
