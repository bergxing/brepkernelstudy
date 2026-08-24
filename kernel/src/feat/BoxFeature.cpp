#include "brep/feat/BoxFeature.h"

#include "brep/Document.h"
#include "brep/Part.h"

#include <algorithm>
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

BoxFeature::BoxFeature(FeatureId id, std::string name, Point3d origin, param::ParameterId length,
                       param::ParameterId width, param::ParameterId height)
    : m_id(id),
      m_name(std::move(name)),
      m_origin(origin),
      m_length(length),
      m_width(width),
      m_height(height)
      {
}

std::unique_ptr<BoxFeature> BoxFeature::Create(param::ParameterStore& store, const BoxSpec& spec)
{
    const double lx = std::abs(spec.Max.x() - spec.Min.x());
    const double hy = std::abs(spec.Max.y() - spec.Min.y());
    const double wz = std::abs(spec.Max.z() - spec.Min.z());
    const Point3d origin{std::min(spec.Min.x(), spec.Max.x()), std::min(spec.Min.y(), spec.Max.y()),
                         std::min(spec.Min.z(), spec.Max.z())};

    const std::string base = spec.Name.empty() ? "Box" : spec.Name;
    FeatureId fid{Guid::Generate()};
    auto length = store.Add(UniqueParamName(store, base + ".Length"), param::ParamKind::Length, lx);
    auto width = store.Add(UniqueParamName(store, base + ".Width"), param::ParamKind::Length, wz);
    auto height = store.Add(UniqueParamName(store, base + ".Height"), param::ParamKind::Length, hy);
    return std::make_unique<BoxFeature>(fid, base, origin, length, width, height);
}

void BoxFeature::CollectParameters(param::ParameterStore& /*store*/)
{
    // Parameters are owned by ParameterStore at creation time.
}

BoxSpec BoxFeature::ToSpec(const param::ParameterStore& params) const
{
    const double lx = params.Get(m_length).value_or(1.0);
    const double wz = params.Get(m_width).value_or(1.0);
    const double hy = params.Get(m_height).value_or(1.0);
    BoxSpec spec;
    spec.Min = m_origin;
    spec.Max = Point3d{m_origin.x() + lx, m_origin.y() + hy, m_origin.z() + wz};
    spec.Name = m_name;
    return spec;
}

bool BoxFeature::Rebuild(Part& part, param::ParameterStore& params)
{
    const BoxSpec spec = ToSpec(params);
    if (std::abs(spec.Max.x() - spec.Min.x()) < 1e-9 || std::abs(spec.Max.y() - spec.Min.y()) < 1e-9 ||
        std::abs(spec.Max.z() - spec.Min.z()) < 1e-9)
    {
        return false;
    }

    Body* body = part.RebuildBoxBody(m_bodyGuid, spec);
    if (!body)
    {
        return false;
    }
    m_bodyGuid = body->Guid;
    return true;
}

}  // namespace brep::feat
