#pragma once

#include "brep/model.hpp"

namespace brep {

struct BoxSpec {
  Vec3 min{0, 0, 0};
  Vec3 max{1, 1, 1};
  double tolerance{1e-7};
  std::string name{"box"};
};

/// Build an axis-aligned solid box as a manifold B-Rep:
/// 1 Body -> 1 closed Shell -> 6 Faces, each with one outer Loop of 4 CoEdges.
Body* make_box(Model& model, const BoxSpec& spec = {});

}  // namespace brep
