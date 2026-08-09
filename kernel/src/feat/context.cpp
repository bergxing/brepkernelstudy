#include "brep/feat/context.hpp"

#include "brep/feat/sketch_feature.hpp"
#include "brep/part.hpp"

namespace brep::feat {

sketch::Sketch* RebuildContext::find_sketch(FeatureId id) const {
  if (!tree) return nullptr;
  IFeature* f = tree->find(id);
  if (!f || f->type_name() != "Sketch") return nullptr;
  return &static_cast<SketchFeature*>(f)->sketch();
}

Body* RebuildContext::find_body(Guid guid) const {
  if (!part || guid.is_nil()) return nullptr;
  for (const auto& body : part->model().bodies()) {
    if (body && body->guid == guid) return body.get();
  }
  return nullptr;
}

}  // namespace brep::feat
