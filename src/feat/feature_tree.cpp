#include "brep/feat/feature_tree.hpp"

namespace brep::feat {

FeatureId FeatureTree::append(std::unique_ptr<IFeature> f) {
  if (!f) return {};
  const FeatureId id = f->id();
  f->set_status(FeatureStatus::Dirty);
  features_.push_back(std::move(f));
  return id;
}

bool FeatureTree::remove(FeatureId id) {
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    if ((*it)->id() == id) {
      features_.erase(it);
      return true;
    }
  }
  return false;
}

IFeature* FeatureTree::find(FeatureId id) {
  for (auto& f : features_) {
    if (f->id() == id) return f.get();
  }
  return nullptr;
}

const IFeature* FeatureTree::find(FeatureId id) const {
  for (const auto& f : features_) {
    if (f->id() == id) return f.get();
  }
  return nullptr;
}

void FeatureTree::mark_dirty_from(FeatureId id) {
  bool mark = false;
  for (auto& f : features_) {
    if (f->id() == id) mark = true;
    if (mark && !f->suppressed()) f->set_status(FeatureStatus::Dirty);
  }
}

void FeatureTree::mark_all_dirty() {
  for (auto& f : features_) {
    if (!f->suppressed()) f->set_status(FeatureStatus::Dirty);
  }
}

int FeatureTree::first_dirty_index() const {
  for (int i = 0; i < static_cast<int>(features_.size()); ++i) {
    if (features_[static_cast<std::size_t>(i)]->status() ==
        FeatureStatus::Dirty) {
      return i;
    }
  }
  return static_cast<int>(features_.size());
}

IFeature* FeatureTree::find_by_body(const Guid& body_guid) {
  if (body_guid.is_nil()) return nullptr;
  for (auto& f : features_) {
    if (f->body_guid() == body_guid) return f.get();
  }
  return nullptr;
}

const IFeature* FeatureTree::find_by_body(const Guid& body_guid) const {
  if (body_guid.is_nil()) return nullptr;
  for (const auto& f : features_) {
    if (f->body_guid() == body_guid) return f.get();
  }
  return nullptr;
}

}  // namespace brep::feat
