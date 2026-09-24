#include "brep/feat/CopiedBodyFeature.h"

#include "brep/bool/TopologyCopy.h"
#include "brep/Guid.h"
#include "brep/Log.h"
#include "brep/Part.h"

namespace brep::feat
{

CopiedBodyFeature::CopiedBodyFeature(FeatureId id, std::string name,
                                     FeatureId source,
                                     RigidTransform transform)
    : m_id(id),
      m_name(std::move(name)),
      m_sourceFeature(source),
      m_transform(transform)
{
}

std::unique_ptr<CopiedBodyFeature> CopiedBodyFeature::Create(
    FeatureId source, RigidTransform transform, std::string name)
{
    if (name.empty())
    {
        name = "Copy";
    }
    return std::make_unique<CopiedBodyFeature>(
        FeatureId{Guid::Generate()}, std::move(name), source, transform);
}

void CopiedBodyFeature::CollectParameters(param::ParameterStore& /*store*/)
{
}

bool CopiedBodyFeature::Rebuild(Part& part, param::ParameterStore& /*params*/)
{
    IFeature* source = part.Features().Find(m_sourceFeature);
    if (!source)
    {
        BREP_WARN("CopiedBodyFeature: source feature {} missing",
                  m_sourceFeature.Guid.ToString());
        return false;
    }
    Body* srcBody = part.FindBody(source->BodyGuid());
    if (!srcBody)
    {
        BREP_WARN("CopiedBodyFeature: source body {} missing",
                  source->BodyGuid().ToString());
        return false;
    }
    if (!m_transform.IsTranslation())
    {
        BREP_WARN("CopiedBodyFeature: only translation is supported");
        return false;
    }

    if (m_bodyGuid.IsValid())
    {
        part.UnregisterBody(m_bodyGuid);
        part.Model().RemoveBody(m_bodyGuid);
    }

    boolean::TopologyCopyContext ctx{part.Model()};
    Body* copied = boolean::CopyBodySubgraph(ctx, *srcBody, "_copy");
    if (!copied)
    {
        return false;
    }
    if (!boolean::ApplyTransformToCopied(ctx, m_transform))
    {
        part.Model().RemoveBody(copied->Guid);
        BREP_WARN("CopiedBodyFeature: transform failed (unsupported geometry)");
        return false;
    }
    if (m_bodyGuid.IsValid())
    {
        copied->Guid = m_bodyGuid;
    }
    part.RegisterBody(*copied);
    m_bodyGuid = copied->Guid;
    return true;
}

}  // namespace brep::feat
