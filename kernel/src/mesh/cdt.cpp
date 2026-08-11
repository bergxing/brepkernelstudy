#include "brep/mesh/cdt.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace brep::mesh {
namespace {

struct Triangle {
  int v[3];
};

[[nodiscard]] double orient2d(const Point2d& a, const Point2d& b,
                              const Point2d& c) {
  return (b.u() - a.u()) * (c.v() - a.v()) -
         (b.v() - a.v()) * (c.u() - a.u());
}

[[nodiscard]] bool in_circumcircle(const Point2d& point, const Triangle& tri,
                                   const std::vector<Point2d>& vertices,
                                   double eps) {
  const Point2d& a = vertices[tri.v[0]];
  const Point2d& b = vertices[tri.v[1]];
  const Point2d& c = vertices[tri.v[2]];
  const double ax = a.u() - point.u();
  const double ay = a.v() - point.v();
  const double bx = b.u() - point.u();
  const double by = b.v() - point.v();
  const double cx = c.u() - point.u();
  const double cy = c.v() - point.v();
  const double determinant =
      (ax * ax + ay * ay) * (bx * cy - by * cx) -
      (bx * bx + by * by) * (ax * cy - ay * cx) +
      (cx * cx + cy * cy) * (ax * by - ay * bx);
  return determinant > eps;
}

}  // namespace

CdtResult triangulate_constrained(
    const std::vector<Point2d>& points,
    const std::vector<std::pair<int, int>>& constraints, double eps) {
  CdtResult result;
  result.vertices.reserve(points.size());
  for (const Point2d& point : points) {
    result.vertices.push_back({point});
  }

  if (!constraints.empty()) {
    result.diagnostics = "Constraint recovery is not implemented";
    return result;
  }
  if (points.size() < 3) {
    result.ok = true;
    return result;
  }

  double min_u = points.front().u();
  double max_u = min_u;
  double min_v = points.front().v();
  double max_v = min_v;
  for (const Point2d& point : points) {
    min_u = std::min(min_u, point.u());
    max_u = std::max(max_u, point.u());
    min_v = std::min(min_v, point.v());
    max_v = std::max(max_v, point.v());
  }

  const double center_u = (min_u + max_u) * 0.5;
  const double center_v = (min_v + max_v) * 0.5;
  const double extent = std::max({max_u - min_u, max_v - min_v, 1.0});

  std::vector<Point2d> vertices = points;
  const int super_begin = static_cast<int>(vertices.size());
  vertices.emplace_back(center_u - 20.0 * extent, center_v - extent);
  vertices.emplace_back(center_u + 20.0 * extent, center_v - extent);
  vertices.emplace_back(center_u, center_v + 20.0 * extent);

  std::vector<Triangle> triangles = {
      {{super_begin, super_begin + 1, super_begin + 2}},
  };
  const double tolerance = std::abs(eps);

  for (int point_index = 0; point_index < super_begin; ++point_index) {
    std::vector<bool> bad(triangles.size(), false);
    std::map<std::pair<int, int>, int> edge_counts;

    for (std::size_t i = 0; i < triangles.size(); ++i) {
      if (!in_circumcircle(vertices[point_index], triangles[i], vertices,
                           tolerance)) {
        continue;
      }
      bad[i] = true;
      for (int edge = 0; edge < 3; ++edge) {
        int a = triangles[i].v[edge];
        int b = triangles[i].v[(edge + 1) % 3];
        if (a > b) {
          std::swap(a, b);
        }
        ++edge_counts[{a, b}];
      }
    }

    std::vector<Triangle> updated;
    updated.reserve(triangles.size() + edge_counts.size());
    for (std::size_t i = 0; i < triangles.size(); ++i) {
      if (!bad[i]) {
        updated.push_back(triangles[i]);
      }
    }
    for (const auto& [edge, count] : edge_counts) {
      if (count != 1) {
        continue;
      }
      int a = edge.first;
      int b = edge.second;
      const double orientation =
          orient2d(vertices[a], vertices[b], vertices[point_index]);
      if (std::abs(orientation) <= tolerance) {
        continue;
      }
      if (orientation < 0.0) {
        std::swap(a, b);
      }
      updated.push_back({{a, b, point_index}});
    }
    triangles = std::move(updated);
  }

  for (const Triangle& triangle : triangles) {
    if (triangle.v[0] >= super_begin || triangle.v[1] >= super_begin ||
        triangle.v[2] >= super_begin) {
      continue;
    }
    result.triangles.push_back(
        {{triangle.v[0], triangle.v[1], triangle.v[2]}});
  }
  result.ok = true;
  return result;
}

}  // namespace brep::mesh
