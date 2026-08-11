#pragma once

#include "brep/bool/context.hpp"
#include "brep/builder.hpp"
#include "brep/math.hpp"
#include "brep/plane.hpp"
#include "brep/topology.hpp"

#include <optional>
#include <string>
#include <vector>

namespace brep::boolean {

/// Linear extrusion of a planar profile (outer + holes) along `plane.normal`.
struct PlanarPrismSpec {
  Plane plane{};
  std::vector<Point2d> outer;
  std::vector<std::vector<Point2d>> holes;
  double d0{0.0};  // height along normal (inclusive)
  double d1{1.0};
  double tolerance{1e-7};
  std::string name{"prism"};
};

/// Recover a straight prism from a closed planar-sided shell (extrude result).
/// Returns nullopt for boxes (prefer `recognize_axis_aligned_box`) or non-prisms.
[[nodiscard]] std::optional<PlanarPrismSpec> recognize_extrusion_prism(
    const Body& body, const BooleanContext& ctx = {});

}  // namespace brep::boolean
