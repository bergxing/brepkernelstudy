#pragma once

#include "brep/math.hpp"

#include <string>
#include <utility>
#include <vector>

namespace brep::mesh {

struct CdtVertex {
  Point2d uv;
};

struct CdtTriangle {
  int v[3];
};

struct CdtResult {
  std::vector<CdtVertex> vertices;
  std::vector<CdtTriangle> triangles;
  bool ok{false};
  std::string diagnostics;
};

/// Triangulate points + optional constraint segments (vertex index pairs).
/// Empty constraints produce the unconstrained Delaunay triangulation.
[[nodiscard]] CdtResult triangulate_constrained(
    const std::vector<Point2d>& points,
    const std::vector<std::pair<int, int>>& constraints,
    double eps = 1e-12);

}  // namespace brep::mesh
