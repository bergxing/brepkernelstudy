#pragma once

#include "brep/bool/Context.h"
#include "brep/Builder.h"
#include "brep/Math.h"
#include "brep/Plane.h"
#include "brep/Topology.h"

#include <optional>
#include <string>
#include <vector>

namespace brep::boolean
{

/// Linear extrusion of a planar profile (outer + holes) along `Plane.Normal`.
struct PlanarPrismSpec
{
  Plane Plane{};
  std::vector<Point2d> Outer;
  std::vector<std::vector<Point2d>> Holes;
  double d0{0.0};  // height along normal (inclusive)
  double d1{1.0};
  double Tolerance{1e-7};
  std::string Name{"prism"};
};

/// Recover a straight prism from a closed planar-sided shell (extrude result).
/// Returns nullopt for boxes (prefer `RecognizeAxisAlignedBox`) or non-prisms.
[[nodiscard]] std::optional<PlanarPrismSpec> RecognizeExtrusionPrism(
    const Body& body, const BooleanContext& ctx = {});

}  // namespace brep::boolean
