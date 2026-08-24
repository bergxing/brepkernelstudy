#include "brep/feat/Regenerator.h"

#include "brep/Part.h"

namespace brep::feat
{

RegenResult Regenerator::Run(Part& part, FeatureTree& tree,
                             param::ParameterStore& params)
{
    RegenResult result;
    const int start = tree.FirstDirtyIndex();
    const int rollback = tree.RollbackIndex();
    const int end = static_cast<int>(tree.Features().size());

    for (int i = start; i < end; ++i)
    {
        if (rollback >= 0 && i > rollback) break;
        IFeature* feature = tree.Features()[static_cast<std::size_t>(i)].get();
        if (!feature) continue;
        if (feature->Suppressed())
        {
            feature->SetStatus(FeatureStatus::Suppressed);
            continue;
        }
        if (!feature->Rebuild(part, params))
        {
            feature->SetStatus(FeatureStatus::Failed);
            result.Ok = false;
            result.Failed = feature->Id();
            result.Message = std::string("Feature rebuild failed: ") +
                             std::string(feature->TypeName());
            return result;
        }
        feature->SetStatus(FeatureStatus::Ok);
    }
    params.ClearDirty();
    return result;
}

}  // namespace brep::feat
