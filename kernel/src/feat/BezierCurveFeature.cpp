#include "brep/feat/BezierCurveFeature.h"

#include "brep/Part.h"

namespace brep::feat
{

BezierCurveFeature::BezierCurveFeature(FeatureId id, std::string name,
                                       BezierSpec spec)
    : m_id(id), m_name(std::move(name))
{
    SetFromSpec(spec);
}

std::unique_ptr<BezierCurveFeature> BezierCurveFeature::Create(
    param::ParameterStore& /*store*/, const BezierSpec& spec)
{
    const std::string base = spec.Name.empty() ? "Bezier" : spec.Name;
    FeatureId fid{Guid::Generate()};
    return std::make_unique<BezierCurveFeature>(fid, base, spec);
}

void BezierCurveFeature::CollectParameters(param::ParameterStore& /*store*/)
{
}

void BezierCurveFeature::SetFromSpec(const BezierSpec& spec)
{
    m_cvs = spec.Cvs;
    m_degree = spec.Degree;
    m_segmentCount = spec.SegmentCount;
    m_corner = spec.Corner;
    m_weights = spec.Weights;
    m_tolerance = spec.Tolerance;
    if (!spec.Name.empty())
    {
        m_name = spec.Name;
    }
}

BezierSpec BezierCurveFeature::ToSpec() const
{
    BezierSpec spec;
    spec.Cvs = m_cvs;
    spec.Degree = m_degree;
    spec.SegmentCount = m_segmentCount;
    spec.Corner = m_corner;
    spec.Weights = m_weights;
    spec.Tolerance = m_tolerance;
    spec.Name = m_name;
    return spec;
}

bool BezierCurveFeature::Rebuild(Part& part, param::ParameterStore& /*params*/)
{
    const BezierSpec spec = ToSpec();
    Body* body = part.RebuildBezierBody(m_bodyGuid, spec);
    if (!body)
    {
        return false;
    }
    m_bodyGuid = body->Guid;
    return true;
}

}  // namespace brep::feat
