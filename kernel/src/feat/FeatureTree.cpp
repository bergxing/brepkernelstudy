#include "brep/feat/FeatureTree.h"

namespace brep::feat
{

FeatureId FeatureTree::Append(std::unique_ptr<IFeature> feature)
{
    if (!feature)
{
        return {};
    }
    const FeatureId id = feature->Id();
    feature->SetStatus(FeatureStatus::Dirty);
    m_features.push_back(std::move(feature));
    return id;
}

bool FeatureTree::Remove(FeatureId id)
{
    for (auto it = m_features.begin(); it != m_features.end(); ++it)
{
        if ((*it)->Id() == id)
{
            m_features.erase(it);
            return true;
        }
    }
    return false;
}

IFeature* FeatureTree::Find(FeatureId id)
{
    for (auto& f : m_features)
{
        if (f->Id() == id)
{
            return f.get();
        }
    }
    return nullptr;
}

const IFeature* FeatureTree::Find(FeatureId id) const
{
    for (const auto& f : m_features)
{
        if (f->Id() == id)
{
            return f.get();
        }
    }
    return nullptr;
}

void FeatureTree::MarkDirtyFrom(FeatureId id)
{
    bool mark = false;
    for (auto& f : m_features)
    {
        if (f->Id() == id)
    {
            mark = true;
        }
        if (mark && !f->Suppressed())
        {
            f->SetStatus(FeatureStatus::Dirty);
        }
    }
}

void FeatureTree::MarkAllDirty()
{
    for (auto& f : m_features)
{
        if (!f->Suppressed())
{
            f->SetStatus(FeatureStatus::Dirty);
        }
    }
}

int FeatureTree::FirstDirtyIndex() const
{
    for (int i = 0; i < static_cast<int>(m_features.size()); ++i)
{
        if (m_features[static_cast<std::size_t>(i)]->Status() == FeatureStatus::Dirty)
{
            return i;
        }
    }
    return static_cast<int>(m_features.size());
}

IFeature* FeatureTree::FindByBody(const Guid& bodyGuid)
{
    if (!bodyGuid.IsValid())
{
        return nullptr;
    }
    for (auto& f : m_features)
    {
        if (f->BodyGuid() == bodyGuid)
    {
            return f.get();
        }
    }
    return nullptr;
}

const IFeature* FeatureTree::FindByBody(const Guid& bodyGuid) const
{
    if (!bodyGuid.IsValid())
{
        return nullptr;
    }
    for (const auto& f : m_features)
    {
        if (f->BodyGuid() == bodyGuid)
    {
            return f.get();
        }
    }
    return nullptr;
}

}  // namespace brep::feat
