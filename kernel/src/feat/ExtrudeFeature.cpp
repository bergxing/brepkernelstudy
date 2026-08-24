#include "brep/feat/ExtrudeFeature.h"

#include "brep/ops/Profile.h"
#include "brep/Part.h"

namespace brep::feat
{

ExtrudeFeature::ExtrudeFeature(FeatureId id, std::string name, FeatureId sketchFeature,
                               param::ParameterId distance)
    : m_id(id), m_name(std::move(name)), m_sketchFeature(sketchFeature), m_distance(distance)
                               {
}

std::unique_ptr<ExtrudeFeature> ExtrudeFeature::Create(param::ParameterStore& store, std::string name,
                                                       FeatureId sketchFeature, double distance)
{
    auto dist = store.Add(name + ".Depth", param::ParamKind::Length, distance);
    FeatureId id{Guid::Generate()};
    return std::make_unique<ExtrudeFeature>(id, std::move(name), sketchFeature, dist);
}

void ExtrudeFeature::CollectParameters(param::ParameterStore& /*store*/)
{
}

bool ExtrudeFeature::Rebuild(Part& part, param::ParameterStore& params)
{
    auto* base = part.Features().Find(m_sketchFeature);
    if (!base || base->TypeName() != "Sketch")
    {
        return false;
    }
    auto* sketchFeat = static_cast<SketchFeature*>(base);

    ops::ExtrudeSpec spec;
    spec.Profile = ops::ExtractProfile(sketchFeat->Sketch());
    spec.Plane = sketchFeat->Sketch().Frame();
    spec.Distance = params.Get(m_distance).value_or(1.0);
    spec.Name = m_name;

    Body* body = part.RebuildExtrudeBody(m_bodyGuid, spec);
    if (!body)
    {
        return false;
    }
    m_bodyGuid = body->Guid;
    return true;
}

}  // namespace brep::feat
