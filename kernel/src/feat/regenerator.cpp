#include "brep/feat/regenerator.hpp"

#include "brep/part.hpp"

namespace brep::feat {

RegenResult Regenerator::run(Part& part, FeatureTree& tree,
                             param::ParameterStore& params) {
  RegenResult result;
  const int start = tree.first_dirty_index();
  const int rollback = tree.rollback_index();
  const int end = static_cast<int>(tree.features().size());

  for (int i = start; i < end; ++i) {
    if (rollback >= 0 && i > rollback) break;
    IFeature* feature = tree.features()[static_cast<std::size_t>(i)].get();
    if (!feature) continue;
    if (feature->suppressed()) {
      feature->set_status(FeatureStatus::Suppressed);
      continue;
    }
    if (!feature->rebuild(part, params)) {
      feature->set_status(FeatureStatus::Failed);
      result.ok = false;
      result.failed = feature->id();
      result.message = std::string("Feature rebuild failed: ") +
                       std::string(feature->type_name());
      return result;
    }
    feature->set_status(FeatureStatus::Ok);
  }
  params.clear_dirty();
  return result;
}

}  // namespace brep::feat
