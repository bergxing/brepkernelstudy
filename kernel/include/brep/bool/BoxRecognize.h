#pragma once

#include "brep/bool/Context.h"
#include "brep/Builder.h"
#include "brep/Topology.h"

#include <optional>

namespace brep::boolean
{

/// Recover axis-aligned box extents from a `MakeBox`-style solid body.
[[nodiscard]] std::optional<BoxSpec> RecognizeAxisAlignedBox(
    const Body& body, const BooleanContext& ctx = {});

}  // namespace brep::boolean
