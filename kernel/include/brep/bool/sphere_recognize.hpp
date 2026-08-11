#pragma once

#include "brep/bool/context.hpp"
#include "brep/builder.hpp"
#include "brep/topology.hpp"

#include <optional>

namespace brep::boolean {

/// Recover analytic sphere from a `make_sphere`-style solid (1 face / seam).
[[nodiscard]] std::optional<SphereSpec> recognize_analytic_sphere(
    const Body& body, const BooleanContext& ctx = {});

}  // namespace brep::boolean
