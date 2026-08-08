#pragma once

#include "brep/feat/feature.hpp"

#include <memory>
#include <vector>

namespace brep::feat {

class FeatureTree {
 public:
  FeatureId append(std::unique_ptr<IFeature> f);
  bool remove(FeatureId id);
  IFeature* find(FeatureId id);
  [[nodiscard]] const IFeature* find(FeatureId id) const;

  [[nodiscard]] const std::vector<std::unique_ptr<IFeature>>& features()
      const noexcept {
    return features_;
  }

  void mark_dirty_from(FeatureId id);
  void mark_all_dirty();
  [[nodiscard]] int first_dirty_index() const;
  [[nodiscard]] int rollback_index() const noexcept { return rollback_index_; }
  void set_rollback(int index) noexcept { rollback_index_ = index; }

  IFeature* find_by_body(const Guid& body_guid);
  [[nodiscard]] const IFeature* find_by_body(const Guid& body_guid) const;

 private:
  std::vector<std::unique_ptr<IFeature>> features_;
  int rollback_index_{-1};  // -1 = all active
};

}  // namespace brep::feat
