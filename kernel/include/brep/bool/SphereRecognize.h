#pragma once

#include "brep/bool/Context.h"
#include "brep/Builder.h"
#include "brep/Topology.h"

#include <optional>

namespace brep::boolean
{

/// Recover analytic sphere from a `MakeSphere`-style solid (1 face / seam).
[[nodiscard]] std::optional<SphereSpec> RecognizeAnalyticSphere(
    const Body& body, const BooleanContext& ctx = {});

}  // namespace brep::boolean
