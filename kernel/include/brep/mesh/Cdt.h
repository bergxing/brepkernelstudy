#pragma once

#include "brep/Math.h"

#include <string>
#include <utility>
#include <vector>

namespace brep::mesh
{

struct CdtVertex
{
  Point2d Uv;
};

struct CdtTriangle
{
  int v[3];
};

struct CdtResult
{
  std::vector<CdtVertex> Vertices;
  std::vector<CdtTriangle> Triangles;
  bool Ok{false};
  std::string Diagnostics;
};

/// Triangulate points + optional constraint segments (vertex index pairs).
/// Empty constraints produce the unconstrained Delaunay triangulation.
[[nodiscard]] CdtResult triangulate_constrained(
    const std::vector<Point2d>& points,
    const std::vector<std::pair<int, int>>& constraints,
    double eps = 1e-12);

/// Triangulate a counter-clockwise outer polygon with clockwise holes.
/// Optional Steiner points are inserted before constraint recovery; they must
/// lie inside the outer and outside all holes to affect the kept mesh.
[[nodiscard]] CdtResult triangulate_polygon_with_holes(
    const std::vector<Point2d>& outer_ccw,
    const std::vector<std::vector<Point2d>>& holes_cw,
    const std::vector<Point2d>& steiner_points = {},
    double eps = 1e-12);

}  // namespace brep::mesh
