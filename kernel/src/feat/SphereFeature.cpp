#include "brep/feat/SphereFeature.h"

#include "brep/Document.h"
#include "brep/Part.h"

#include <cmath>

namespace brep::feat
{
namespace
{

std::string UniqueParamName(param::ParameterStore& store, std::string base)
{
    if (!store.FindByName(base))
{
        return base;
    }
    for (int i = 2; i < 10000; ++i)
    {
        std::string candidate = base + "_" + std::to_string(i);
        if (!store.FindByName(candidate))
        {
            return candidate;
        }
    }
    return base + "_" + Guid::Generate().ToString();
}

}  // namespace

SphereFeature::SphereFeature(FeatureId id, std::string name, Point3d center,
                             param::ParameterId radius)
    : m_id(id), m_name(std::move(name)), m_center(center), m_radius(radius)
                             {
}

std::unique_ptr<SphereFeature> SphereFeature::Create(param::ParameterStore& store,
                                                     const SphereSpec& spec)
{
    const std::string base = spec.Name.empty() ? "Sphere" : spec.Name;
    FeatureId fid{Guid::Generate()};
    auto radius = store.Add(UniqueParamName(store, base + ".Radius"), param::ParamKind::Length,
                            std::abs(spec.Radius));
    return std::make_unique<SphereFeature>(fid, base, spec.Center, radius);
}

void SphereFeature::CollectParameters(param::ParameterStore& /*store*/)
{
}

SphereSpec SphereFeature::ToSpec(const param::ParameterStore& params) const
{
    SphereSpec spec;
    spec.Center = m_center;
    spec.Radius = params.Get(m_radius).value_or(1.0);
    spec.Name = m_name;
    return spec;
}

bool SphereFeature::Rebuild(Part& part, param::ParameterStore& params)
{
    const SphereSpec spec = ToSpec(params);
    if (!(spec.Radius > 1e-9))
    {
        return false;
    }

    Body* body = part.RebuildSphereBody(m_bodyGuid, spec);
    if (!body)
    {
        return false;
    }
    m_bodyGuid = body->Guid;
    return true;
}

}  // namespace brep::feat
