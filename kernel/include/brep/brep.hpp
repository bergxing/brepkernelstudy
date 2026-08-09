#pragma once

// Deprecated umbrella. Prefer graded headers:
//   #include "api/core.hpp"
//   #include "api/mesh.hpp"
//   #include "api/modeling.hpp"
//   #include "api/persistence.hpp"
#if defined(_MSC_VER)
#pragma message( \
    "brep/brep.hpp is deprecated; include api/core.hpp, api/mesh.hpp, api/modeling.hpp, or api/persistence.hpp instead")
#elif defined(__GNUC__) || defined(__clang__)
#warning \
    "brep/brep.hpp is deprecated; include api/core.hpp, api/mesh.hpp, api/modeling.hpp, or api/persistence.hpp instead"
#endif

#include "brep/asm/assembly.hpp"
#include "brep/builder.hpp"
#include "brep/document.hpp"
#include "brep/dump.hpp"
#include "brep/feat/box_feature.hpp"
#include "brep/feat/extrude_feature.hpp"
#include "brep/feat/feature.hpp"
#include "brep/feat/feature_history.hpp"
#include "brep/feat/feature_tree.hpp"
#include "brep/feat/regenerator.hpp"
#include "brep/feat/sketch_feature.hpp"
#include "brep/geometry.hpp"
#include "brep/guid.hpp"
#include "brep/io/bks_cache.hpp"
#include "brep/io/xl_document.hpp"
#include "brep/iobject.hpp"
#include "brep/log.hpp"
#include "brep/material.hpp"
#include "brep/math.hpp"
#include "brep/mesh.hpp"
#include "brep/model.hpp"
#include "brep/naming/topology_ref.hpp"
#include "brep/object_registry.hpp"
#include "brep/ops/profile.hpp"
#include "brep/param/parameter.hpp"
#include "brep/part.hpp"
#include "brep/plane.hpp"
#include "brep/sketch/sketch.hpp"
#include "brep/solve2d/solver.hpp"
#include "brep/topology.hpp"
#include "brep/types.hpp"
#include "brep/validate.hpp"

namespace brep {

[[deprecated(
    "Include api/core.hpp, api/mesh.hpp, api/modeling.hpp, or "
    "api/persistence.hpp instead of brep/brep.hpp")]]
inline constexpr bool k_brep_umbrella_deprecated = true;

}  // namespace brep
