#pragma once

#include "brep/feat/feature.hpp"

#include <string>

namespace brep::naming {

/// Stable topology reference across feature regeneration.
///
/// Convention (T4.3): `local_name` is a stable, feature-local token — prefer
/// snake_case roles (`end_face`, `outer_loop`, `seam_edge`) over positional
/// indices when the role survives rebuild; use `edge_<i>` / `face_<i>` only
/// for anonymous topology. Empty `local_name` means "feature body root".
struct TopologyRef {
  feat::FeatureId feature{};
  std::string local_name;
};

}  // namespace brep::naming
