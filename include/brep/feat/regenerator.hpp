#pragma once

#include "brep/feat/feature_tree.hpp"
#include "brep/param/parameter.hpp"

#include <string>

namespace brep {
class Part;
}

namespace brep::feat {

struct RegenResult {
  bool ok{true};
  FeatureId failed{};
  std::string message;
};

class Regenerator {
 public:
  static RegenResult run(Part& part, FeatureTree& tree,
                         param::ParameterStore& params);
};

}  // namespace brep::feat
