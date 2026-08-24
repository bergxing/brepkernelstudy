#pragma once

// Deprecated umbrella. Prefer graded headers:
//   #include "api/Core.h"
//   #include "api/Mesh.h"
//   #include "api/Modeling.h"
//   #include "api/Persistence.h"
#if defined(_MSC_VER)
#pragma message( \
    "brep/Brep.h is deprecated; include api/Core.h, api/Mesh.h, api/Modeling.h, or api/Persistence.h instead")
#elif defined(__GNUC__) || defined(__clang__)
#warning \
    "brep/Brep.h is deprecated; include api/Core.h, api/Mesh.h, api/Modeling.h, or api/Persistence.h instead"
#endif

#include "brep/asm/Assembly.h"
#include "brep/Builder.h"
#include "brep/Document.h"
#include "brep/Dump.h"
#include "brep/feat/BoxFeature.h"
#include "brep/feat/ExtrudeFeature.h"
#include "brep/feat/Feature.h"
#include "brep/feat/FeatureHistory.h"
#include "brep/feat/FeatureTree.h"
#include "brep/feat/Regenerator.h"
#include "brep/feat/SketchFeature.h"
#include "brep/feat/SphereFeature.h"
#include "brep/Geometry.h"
#include "brep/Guid.h"
#include "brep/io/BksCache.h"
#include "brep/io/XlDocument.h"
#include "brep/IObject.h"
#include "brep/Log.h"
#include "brep/Material.h"
#include "brep/Math.h"
#include "brep/Mesh.h"
#include "brep/Model.h"
#include "brep/naming/TopologyRef.h"
#include "brep/ObjectRegistry.h"
#include "brep/ops/Profile.h"
#include "brep/param/Parameter.h"
#include "brep/Part.h"
#include "brep/Plane.h"
#include "brep/sketch/Sketch.h"
#include "brep/solve2d/Solver.h"
#include "brep/Topology.h"
#include "brep/Types.h"
#include "brep/Validate.h"

namespace brep
{

[[deprecated(
    "Include api/Core.h, api/Mesh.h, api/Modeling.h, or "
    "api/Persistence.h instead of brep/Brep.h")]]
inline constexpr bool k_brep_umbrella_deprecated = true;

}  // namespace brep
