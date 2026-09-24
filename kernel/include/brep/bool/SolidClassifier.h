#pragma once

#include "brep/bool/Classify.h"
#include "brep/Topology.h"

namespace brep::boolean
{

/// Generic point-in-solid test for closed B-Rep bodies (Pipeline Classify stage).
/// Uses ray parity against face surfaces by `SurfaceKind` — no body-level Recognize.
[[nodiscard]] SolidClass ClassifyPointInBody(const Body& solid, const Point3d& p,
                                               double eps);

}  // namespace brep::boolean
