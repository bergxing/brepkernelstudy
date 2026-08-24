#pragma once

#include "brep/feat/FeatureTree.h"
#include "brep/param/Parameter.h"

#include <string>

namespace brep
{
class Part;
}

namespace brep::feat
{

struct RegenResult
{
    bool Ok{true};
    FeatureId Failed{};
    std::string Message;
};

class Regenerator
{
public:
    static RegenResult Run(Part& part, FeatureTree& tree,
                           param::ParameterStore& params);
};

}  // namespace brep::feat
