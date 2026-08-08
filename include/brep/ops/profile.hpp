#pragma once

#include "brep/math.hpp"
#include "brep/model.hpp"
#include "brep/plane.hpp"
#include "brep/sketch/sketch.hpp"

#include <string>
#include <vector>

namespace brep::ops {

struct Profile2d {
  std::vector<Point2d> outer;
  std::vector<std::vector<Point2d>> holes;
};

/// Extract a closed outer polyline from sketch lines (point order heuristic).
Profile2d extract_profile(const sketch::Sketch& sketch);

struct ExtrudeSpec {
  Profile2d profile;
  Plane plane{Plane::xz_y_up()};
  double distance{1.0};
  bool symmetric{false};
  double tolerance{1e-7};
  std::string name{"extrude"};
};

/// Extrude a planar profile. Axis-aligned rectangular profiles become boxes;
/// other closed polylines use a prism builder (planar sides).
Body* extrude(Model& model, const ExtrudeSpec& spec);

}  // namespace brep::ops
