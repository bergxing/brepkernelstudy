#include "brep/mesh/cdt.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <set>

namespace brep::mesh {
namespace {

struct Triangle {
  int v[3];
};

using Edge = std::pair<int, int>;

[[nodiscard]] Edge edge_key(int a, int b) {
  if (a > b) {
    std::swap(a, b);
  }
  return {a, b};
}

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

[[nodiscard]] Triangle make_ccw_triangle(int a, int b, int c,
                                         const std::vector<Point2d>& vertices) {
  if (orient2d(vertices[a], vertices[b], vertices[c]) < 0.0) {
    std::swap(a, b);
  }
  return {{a, b, c}};
}

[[nodiscard]] bool point_in_triangle(const Point2d& point,
                                     const Triangle& triangle,
                                     const std::vector<Point2d>& vertices,
                                     double eps) {
  return orient2d(vertices[triangle.v[0]], vertices[triangle.v[1]], point) >=
             -eps &&
         orient2d(vertices[triangle.v[1]], vertices[triangle.v[2]], point) >=
             -eps &&
         orient2d(vertices[triangle.v[2]], vertices[triangle.v[0]], point) >=
             -eps;
}

[[nodiscard]] std::vector<Triangle> delaunay_triangulation(
    const std::vector<Point2d>& points, double eps) {
  if (points.size() < 3) {
    return {};
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

  for (int point_index = 0; point_index < super_begin; ++point_index) {
    int seed = -1;
    for (std::size_t i = 0; i < triangles.size(); ++i) {
      if (point_in_triangle(vertices[point_index], triangles[i], vertices,
                            eps)) {
        seed = static_cast<int>(i);
        break;
      }
    }
    if (seed < 0) {
      continue;
    }

    std::vector<std::array<int, 3>> neighbors(
        triangles.size(), std::array<int, 3>{-1, -1, -1});
    std::map<Edge, std::pair<int, int>> first_owner;
    for (std::size_t i = 0; i < triangles.size(); ++i) {
      for (int edge = 0; edge < 3; ++edge) {
        const Edge key =
            edge_key(triangles[i].v[edge], triangles[i].v[(edge + 1) % 3]);
        const auto [it, inserted] =
            first_owner.emplace(key, std::pair{static_cast<int>(i), edge});
        if (!inserted) {
          const auto [other_triangle, other_edge] = it->second;
          neighbors[i][edge] = other_triangle;
          neighbors[other_triangle][other_edge] = static_cast<int>(i);
        }
      }
    }

    std::vector<bool> visited(triangles.size(), false);
    std::vector<bool> bad(triangles.size(), false);
    std::deque<int> queue;
    queue.push_back(seed);
    while (!queue.empty()) {
      const int current = queue.front();
      queue.pop_front();
      if (visited[current]) {
        continue;
      }
      visited[current] = true;
      if (!in_circumcircle(vertices[point_index], triangles[current], vertices,
                           eps)) {
        continue;
      }
      bad[current] = true;
      for (const int neighbor : neighbors[current]) {
        if (neighbor >= 0 && !visited[neighbor]) {
          queue.push_back(neighbor);
        }
      }
    }

    std::map<Edge, int> edge_counts;
    for (std::size_t i = 0; i < triangles.size(); ++i) {
      if (!bad[i]) {
        continue;
      }
      for (int edge = 0; edge < 3; ++edge) {
        ++edge_counts[edge_key(triangles[i].v[edge],
                               triangles[i].v[(edge + 1) % 3])];
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
      if (count == 1 &&
          std::abs(orient2d(vertices[edge.first], vertices[edge.second],
                            vertices[point_index])) > eps) {
        updated.push_back(
            make_ccw_triangle(edge.first, edge.second, point_index, vertices));
      }
    }
    triangles = std::move(updated);
  }

  triangles.erase(
      std::remove_if(triangles.begin(), triangles.end(),
                     [super_begin](const Triangle& triangle) {
                       return triangle.v[0] >= super_begin ||
                              triangle.v[1] >= super_begin ||
                              triangle.v[2] >= super_begin;
                     }),
      triangles.end());
  return triangles;
}

[[nodiscard]] std::map<Edge, std::vector<int>> edge_owners(
    const std::vector<Triangle>& triangles) {
  std::map<Edge, std::vector<int>> owners;
  for (std::size_t i = 0; i < triangles.size(); ++i) {
    for (int edge = 0; edge < 3; ++edge) {
      owners[edge_key(triangles[i].v[edge],
                      triangles[i].v[(edge + 1) % 3])]
          .push_back(static_cast<int>(i));
    }
  }
  return owners;
}

[[nodiscard]] int opposite_vertex(const Triangle& triangle, const Edge& edge) {
  for (const int vertex : triangle.v) {
    if (vertex != edge.first && vertex != edge.second) {
      return vertex;
    }
  }
  return -1;
}

[[nodiscard]] bool segments_properly_intersect(
    const Point2d& a, const Point2d& b, const Point2d& c, const Point2d& d,
    double eps) {
  const double ab_c = orient2d(a, b, c);
  const double ab_d = orient2d(a, b, d);
  const double cd_a = orient2d(c, d, a);
  const double cd_b = orient2d(c, d, b);
  return ((ab_c > eps && ab_d < -eps) || (ab_c < -eps && ab_d > eps)) &&
         ((cd_a > eps && cd_b < -eps) || (cd_a < -eps && cd_b > eps));
}

[[nodiscard]] int interior_vertex_on_segment(
    int a, int b, const std::vector<Point2d>& vertices, double eps) {
  const Point2d& start = vertices[a];
  const Point2d& end = vertices[b];
  int best = -1;
  double best_distance = 0.0;
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    if (static_cast<int>(i) == a || static_cast<int>(i) == b ||
        std::abs(orient2d(start, end, vertices[i])) > eps) {
      continue;
    }
    const double du = vertices[i].u() - start.u();
    const double dv = vertices[i].v() - start.v();
    const double eu = vertices[i].u() - end.u();
    const double ev = vertices[i].v() - end.v();
    if (du * eu + dv * ev >= -eps) {
      continue;
    }
    const double distance = du * du + dv * dv;
    if (best < 0 || distance < best_distance) {
      best = static_cast<int>(i);
      best_distance = distance;
    }
  }
  return best;
}

[[nodiscard]] bool flip_intersecting_edge(
    const Edge& constraint, std::vector<Triangle>& triangles,
    const std::vector<Point2d>& vertices,
    const std::set<Edge>& constrained_edges, double eps) {
  const auto owners = edge_owners(triangles);
  for (const auto& [edge, adjacent] : owners) {
    if (adjacent.size() != 2 || constrained_edges.contains(edge) ||
        edge.first == constraint.first || edge.first == constraint.second ||
        edge.second == constraint.first || edge.second == constraint.second ||
        !segments_properly_intersect(
            vertices[constraint.first], vertices[constraint.second],
            vertices[edge.first], vertices[edge.second], eps)) {
      continue;
    }

    const int c = opposite_vertex(triangles[adjacent[0]], edge);
    const int d = opposite_vertex(triangles[adjacent[1]], edge);
    if (c < 0 || d < 0 || owners.contains(edge_key(c, d))) {
      continue;
    }
    // Replacing an intersecting diagonal with another intersecting diagonal can
    // flip the same quadrilateral back on the next iteration.  Only accept a
    // flip that strictly decreases the number of mesh edges properly
    // intersecting the constraint; otherwise use the Steiner fallback.
    if (segments_properly_intersect(
            vertices[constraint.first], vertices[constraint.second],
            vertices[c], vertices[d], eps)) {
      continue;
    }
    const double side_c =
        orient2d(vertices[c], vertices[d], vertices[edge.first]);
    const double side_d =
        orient2d(vertices[c], vertices[d], vertices[edge.second]);
    if (!((side_c > eps && side_d < -eps) ||
          (side_c < -eps && side_d > eps))) {
      continue;
    }

    triangles[adjacent[0]] =
        make_ccw_triangle(c, d, edge.first, vertices);
    triangles[adjacent[1]] =
        make_ccw_triangle(d, c, edge.second, vertices);
    return true;
  }
  return false;
}

[[nodiscard]] bool point_in_polygon(const Point2d& point,
                                    const std::vector<Point2d>& polygon) {
  bool inside = false;
  for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const Point2d& a = polygon[i];
    const Point2d& b = polygon[j];
    if ((a.v() > point.v()) != (b.v() > point.v()) &&
        point.u() < (b.u() - a.u()) * (point.v() - a.v()) /
                            (b.v() - a.v()) +
                        a.u()) {
      inside = !inside;
    }
  }
  return inside;
}

}  // namespace

CdtResult triangulate_constrained(
    const std::vector<Point2d>& points,
    const std::vector<std::pair<int, int>>& constraints, double eps) {
  CdtResult result;
  const double tolerance = std::abs(eps);
  std::vector<Point2d> vertices = points;
  std::vector<Edge> segments;
  segments.reserve(constraints.size());
  for (const auto& [a, b] : constraints) {
    if (a < 0 || b < 0 || a >= static_cast<int>(points.size()) ||
        b >= static_cast<int>(points.size()) || a == b) {
      result.diagnostics = "Invalid constraint endpoint";
      return result;
    }
    segments.push_back(edge_key(a, b));
  }

  std::vector<Triangle> triangles =
      delaunay_triangulation(vertices, tolerance);
  std::set<Edge> constrained_edges;
  std::size_t segment_index = 0;
  int steiner_count = 0;
  int flips_since_restart = 0;
  while (segment_index < segments.size()) {
    const Edge segment = segments[segment_index];
    if (edge_owners(triangles).contains(segment)) {
      constrained_edges.insert(segment);
      ++segment_index;
      flips_since_restart = 0;
      continue;
    }

    const int split_vertex =
        interior_vertex_on_segment(segment.first, segment.second, vertices,
                                   tolerance);
    if (split_vertex >= 0) {
      segments[segment_index] = edge_key(segment.first, split_vertex);
      segments.insert(segments.begin() +
                          static_cast<std::ptrdiff_t>(segment_index + 1),
                      edge_key(split_vertex, segment.second));
      continue;
    }

    if (flips_since_restart < 10000 &&
        flip_intersecting_edge(segment, triangles, vertices, constrained_edges,
                               tolerance)) {
      ++flips_since_restart;
      continue;
    }

    if (++steiner_count > 128) {
      result.diagnostics = "Constraint recovery exceeded Steiner point limit";
      return result;
    }
    const Point2d& a = vertices[segment.first];
    const Point2d& b = vertices[segment.second];
    const Point2d midpoint{(a.u() + b.u()) * 0.5,
                           (a.v() + b.v()) * 0.5};
    vertices.push_back(midpoint);
    const int midpoint_index = static_cast<int>(vertices.size()) - 1;
    segments[segment_index] = edge_key(segment.first, midpoint_index);
    segments.insert(
        segments.begin() + static_cast<std::ptrdiff_t>(segment_index + 1),
        edge_key(midpoint_index, segment.second));
    triangles = delaunay_triangulation(vertices, tolerance);
    constrained_edges.clear();
    segment_index = 0;
    flips_since_restart = 0;
  }

  result.vertices.reserve(vertices.size());
  for (const Point2d& point : vertices) {
    result.vertices.push_back({point});
  }
  if (vertices.size() < 3) {
    result.ok = true;
    return result;
  }
  for (const Triangle& triangle : triangles) {
    result.triangles.push_back(
        {{triangle.v[0], triangle.v[1], triangle.v[2]}});
  }
  result.ok = true;
  return result;
}

CdtResult triangulate_polygon_with_holes(
    const std::vector<Point2d>& outer_ccw,
    const std::vector<std::vector<Point2d>>& holes_cw, double eps) {
  CdtResult result;
  if (outer_ccw.size() < 3) {
    result.diagnostics = "Outer polygon must have at least three vertices";
    return result;
  }

  std::vector<Point2d> points;
  std::vector<Edge> constraints;
  const auto append_ring = [&](const std::vector<Point2d>& ring) {
    const int first = static_cast<int>(points.size());
    points.insert(points.end(), ring.begin(), ring.end());
    for (std::size_t i = 0; i < ring.size(); ++i) {
      constraints.emplace_back(
          first + static_cast<int>(i),
          first + static_cast<int>((i + 1) % ring.size()));
    }
  };

  append_ring(outer_ccw);
  for (const auto& hole : holes_cw) {
    if (hole.size() < 3) {
      result.diagnostics = "Hole polygon must have at least three vertices";
      return result;
    }
    append_ring(hole);
  }

  result = triangulate_constrained(points, constraints, eps);
  if (!result.ok) {
    return result;
  }
  result.triangles.erase(
      std::remove_if(
          result.triangles.begin(), result.triangles.end(),
          [&](const CdtTriangle& triangle) {
            const Point2d& a = result.vertices[triangle.v[0]].uv;
            const Point2d& b = result.vertices[triangle.v[1]].uv;
            const Point2d& c = result.vertices[triangle.v[2]].uv;
            const Point2d centroid{(a.u() + b.u() + c.u()) / 3.0,
                                   (a.v() + b.v() + c.v()) / 3.0};
            if (!point_in_polygon(centroid, outer_ccw)) {
              return true;
            }
            return std::any_of(
                holes_cw.begin(), holes_cw.end(),
                [&](const std::vector<Point2d>& hole) {
                  return point_in_polygon(centroid, hole);
                });
          }),
      result.triangles.end());
  return result;
}

}  // namespace brep::mesh
