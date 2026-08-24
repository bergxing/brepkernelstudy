#pragma once

// Umbrella header for boolean evaluation types and evaluator interface.

#include "brep/bool/Context.h"
#include "brep/bool/Evaluator.h"
#include "brep/bool/IntersectPlanePlane.h"
#include "brep/bool/IntersectPlaneSphere.h"
#include "brep/bool/IntersectSphereSphere.h"
#include "brep/bool/IntersectPlaneCylinder.h"
#include "brep/bool/IntersectSphereCylinder.h"
#include "brep/bool/BoxBoolean.h"
#include "brep/bool/PlanarBoolean.h"
#include "brep/bool/PlanarRecognize.h"
#include "brep/bool/SphereBoxBoolean.h"
#include "brep/bool/SphereRecognize.h"
#include "brep/bool/SphereSphereBoolean.h"
#include "brep/bool/Classify.h"
#include "brep/bool/CompositeEvaluator.h"
#include "brep/bool/FastPath.h"
#include "brep/bool/Pipeline.h"
#include "brep/bool/Broadphase.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
