#pragma once

#include "brep/feat/feature_tree.hpp"
#include "brep/guid.hpp"
#include "brep/param/parameter.hpp"

namespace brep {
class Document;
class Part;
class Body;
}  // namespace brep

namespace brep::sketch {
class Sketch;
}

namespace brep::feat {

struct RebuildContext {
  Document* document{nullptr};
  Part* part{nullptr};
  param::ParameterStore* params{nullptr};
  FeatureTree* tree{nullptr};

  [[nodiscard]] sketch::Sketch* find_sketch(FeatureId id) const;
  [[nodiscard]] Body* find_body(Guid guid) const;
};

}  // namespace brep::feat
