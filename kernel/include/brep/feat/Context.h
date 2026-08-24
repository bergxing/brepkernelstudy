#pragma once

#include "brep/feat/FeatureTree.h"
#include "brep/Guid.h"
#include "brep/param/Parameter.h"

namespace brep
{
class Document;
class Part;
class Body;
}  // namespace brep

namespace brep::sketch
{
class Sketch;
}

namespace brep::feat
{

struct RebuildContext
{
    brep::Document* Document{nullptr};
    brep::Part* Part{nullptr};
    param::ParameterStore* Params{nullptr};
    FeatureTree* Tree{nullptr};

    [[nodiscard]] sketch::Sketch* FindSketch(FeatureId id) const;
    [[nodiscard]] brep::Body* FindBody(brep::Guid guid) const;
};

}  // namespace brep::feat
