#pragma once

#include "brep/feat/Feature.h"

#include <memory>
#include <vector>

namespace brep::feat
{

class FeatureTree
{
 public:
  FeatureId Append(std::unique_ptr<IFeature> f);
  bool Remove(FeatureId id);
  IFeature* Find(FeatureId id);
  [[nodiscard]] const IFeature* Find(FeatureId id) const;

  [[nodiscard]] const std::vector<std::unique_ptr<IFeature>>& Features()
      const noexcept
  {
    return m_features;
  }

  void MarkDirtyFrom(FeatureId id);
  void MarkAllDirty();
  [[nodiscard]] int FirstDirtyIndex() const;
  [[nodiscard]] int RollbackIndex() const noexcept
  {
    return m_rollbackIndex;
  }
  void SetRollback(int index) noexcept
  {
    m_rollbackIndex = index;
  }

  IFeature* FindByBody(const Guid& body_guid);
  [[nodiscard]] const IFeature* FindByBody(const Guid& body_guid) const;

 private:
  std::vector<std::unique_ptr<IFeature>> m_features;
  int m_rollbackIndex{-1};  // -1 = all active
};

}  // namespace brep::feat
