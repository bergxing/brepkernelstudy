#pragma once

#include "brep/feat/feature.hpp"

#include <string>

namespace brep::naming {

/// Stable topology reference across feature regeneration.
struct TopologyRef {
  feat::FeatureId feature{};
  std::string local_name;  // e.g. "end_face", "edge_0"
};

}  // namespace brep::naming
