#pragma once

// Deprecated umbrella for BoxSpec / SphereSpec. Prefer:
//   #include "brep/feat/PrimitiveSpecs.h"  (kernel)
//   #include "api/Modeling.h"              (viewer / examples)
#if defined(_MSC_VER)
#pragma message("brep/Builder.h is deprecated; include api/Modeling.h or brep/feat/PrimitiveSpecs.h")
#elif defined(__GNUC__) || defined(__clang__)
#warning "brep/Builder.h is deprecated; include api/Modeling.h or brep/feat/PrimitiveSpecs.h"
#endif

#include "brep/feat/PrimitiveSpecs.h"
