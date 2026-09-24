#include "brep/feat/NurbsCurveFeature.h"

#include "brep/Geometry.h"
#include "brep/Part.h"

namespace brep::feat
{

NurbsCurveFeature::NurbsCurveFeature(FeatureId id, std::string name,
                                     NurbsCurveSpec spec)
    : m_id(id), m_name(std::move(name))
{
    SetFromSpec(spec);
}

std::unique_ptr<NurbsCurveFeature> NurbsCurveFeature::Create(
    param::ParameterStore& /*store*/, const NurbsCurveSpec& spec)
{
    const std::string base = spec.Name.empty() ? "NurbsCurve" : spec.Name;
    FeatureId fid{Guid::Generate()};
    return std::make_unique<NurbsCurveFeature>(fid, base, spec);
}

void NurbsCurveFeature::CollectParameters(param::ParameterStore& /*store*/)
{
}

void NurbsCurveFeature::SetFromSpec(const NurbsCurveSpec& spec)
{
    m_cvs = spec.Cvs;
    m_weights = spec.Weights;
    m_degree = spec.Degree;
    m_tolerance = spec.Tolerance;
    if (spec.Knots.empty())
    {
        m_knots = ClampedUniformKnots(static_cast<int>(spec.Cvs.size()),
                                      spec.Degree);
    }
    else
    {
        m_knots = spec.Knots;
    }
    if (!spec.Name.empty())
    {
        m_name = spec.Name;
    }
}

NurbsCurveSpec NurbsCurveFeature::ToSpec() const
{
    NurbsCurveSpec spec;
    spec.Cvs = m_cvs;
    spec.Weights = m_weights;
    spec.Knots = m_knots;
    spec.Degree = m_degree;
    spec.Tolerance = m_tolerance;
    spec.Name = m_name;
    return spec;
}

bool NurbsCurveFeature::Rebuild(Part& part, param::ParameterStore& /*params*/)
{
    const NurbsCurveSpec spec = ToSpec();
    Body* body = part.RebuildNurbsCurveBody(m_bodyGuid, spec);
    if (!body)
    {
        return false;
    }
    m_bodyGuid = body->Guid;
    return true;
}

}  // namespace brep::feat
