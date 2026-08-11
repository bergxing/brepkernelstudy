#pragma once

#include "brep/bool/context.hpp"
#include "brep/builder.hpp"
#include "brep/topology.hpp"

#include <optional>

namespace brep::boolean {

/// Recover axis-aligned box extents from a `make_box`-style solid body.
[[nodiscard]] std::optional<BoxSpec> recognize_axis_aligned_box(
    const Body& body, const BooleanContext& ctx = {});

}  // namespace brep::boolean
