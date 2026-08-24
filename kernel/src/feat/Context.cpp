#include "brep/feat/Context.h"

#include "brep/feat/SketchFeature.h"
#include "brep/Part.h"

namespace brep::feat
{

sketch::Sketch* RebuildContext::FindSketch(FeatureId id) const
{
    if (!Tree) return nullptr;
    IFeature* f = Tree->Find(id);
    if (!f || f->TypeName() != "Sketch") return nullptr;
    return &static_cast<SketchFeature*>(f)->Sketch();
}

Body* RebuildContext::FindBody(Guid guid) const
{
    if (!Part || !guid.IsValid()) return nullptr;
    for (const auto& body : Part->Model().Bodies())
    {
        if (body && body->Guid == guid) return body.get();
    }
    return nullptr;
}

}  // namespace brep::feat
