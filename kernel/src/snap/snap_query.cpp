#include "brep/snap/snap_query.hpp"

#include "brep/geometry.hpp"
#include "brep/topology.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_set>
#include <vector>

namespace brep {
namespace {

bool kind_enabled(const SnapQuery& query, SnapKind kind) {
  return (query.kinds & static_cast<std::uint32_t>(kind)) != 0;
}

bool points_near(const Point3d& a, const Point3d& b, double tolerance) {
  return (a - b).norm() <= std::max(0.0, tolerance);
}

Point3d closest_point_on_segment(const Point3d& point, const Point3d& a,
                                 const Point3d& b) {
  const Vector3d direction = b - a;
  const double length_squared = direction.squaredNorm();
  if (length_squared <= 0.0) return a;

  const double parameter =
      std::clamp((point - a).dot(direction) / length_squared, 0.0, 1.0);
  return a + parameter * direction;
}

struct SegmentClosestPoints {
  Point3d first;
  Point3d second;
  double distance;
};

SegmentClosestPoints segment_closest_points(const Point3d& a0,
                                            const Point3d& a1,
                                            const Point3d& b0,
                                            const Point3d& b1) {
  const Vector3d first_direction = a1 - a0;
  const Vector3d second_direction = b1 - b0;
  const Vector3d origins = a0 - b0;
  const double first_length_squared = first_direction.squaredNorm();
  const double second_length_squared = second_direction.squaredNorm();
  const double second_projection = second_direction.dot(origins);

  double first_parameter = 0.0;
  double second_parameter = 0.0;
  if (first_length_squared <= 0.0 && second_length_squared <= 0.0) {
    return {a0, b0, (a0 - b0).norm()};
  }
  if (first_length_squared <= 0.0) {
    second_parameter =
        std::clamp(second_projection / second_length_squared, 0.0, 1.0);
  } else {
    const double first_projection = first_direction.dot(origins);
    if (second_length_squared <= 0.0) {
      first_parameter =
          std::clamp(-first_projection / first_length_squared, 0.0, 1.0);
    } else {
      const double directions_dot =
          first_direction.dot(second_direction);
      const double denominator =
          first_length_squared * second_length_squared -
          directions_dot * directions_dot;
      if (denominator != 0.0) {
        first_parameter =
            std::clamp((directions_dot * second_projection -
                        first_projection * second_length_squared) /
                           denominator,
                       0.0, 1.0);
      }

      second_parameter =
          (directions_dot * first_parameter + second_projection) /
          second_length_squared;
      if (second_parameter < 0.0) {
        second_parameter = 0.0;
        first_parameter =
            std::clamp(-first_projection / first_length_squared, 0.0, 1.0);
      } else if (second_parameter > 1.0) {
        second_parameter = 1.0;
        first_parameter = std::clamp(
            (directions_dot - first_projection) / first_length_squared, 0.0,
            1.0);
      }
    }
  }

  const Point3d first = a0 + first_parameter * first_direction;
  const Point3d second = b0 + second_parameter * second_direction;
  return {first, second, (first - second).norm()};
}

void append_candidate(std::vector<SnapCandidate>& candidates, SnapKind kind,
                      const Point3d& point, const Body& body,
                      double tolerance) {
  const bool duplicate =
      std::any_of(candidates.begin(), candidates.end(), [&](const auto& candidate) {
        return candidate.kind == kind && candidate.body_guid == body.guid &&
               points_near(candidate.point, point, tolerance);
      });
  if (!duplicate) candidates.push_back({kind, point, body.guid});
}

std::vector<Point3d> outer_loop_vertices(const Face& face, double tolerance) {
  std::vector<Point3d> positions;
  const Loop* loop = face.outer_loop();
  if (!loop) return positions;

  loop->for_each_coedge([&](const CoEdge& coedge) {
    const Vertex* vertex = coedge.from();
    if (!vertex) return;
    const Point3d& point = vertex->position();
    const bool duplicate =
        std::any_of(positions.begin(), positions.end(), [&](const Point3d& other) {
          return points_near(point, other, tolerance);
        });
    if (!duplicate) positions.push_back(point);
  });
  return positions;
}

Point3d average_points(const std::vector<Point3d>& points) {
  Vector3d sum;
  for (const Point3d& point : points) {
    sum += Vector3d{point.x(), point.y(), point.z()};
  }
  const Vector3d average = sum / static_cast<double>(points.size());
  return Point3d{average.x(), average.y(), average.z()};
}

void append_sphere_center(std::vector<SnapCandidate>& candidates,
                          const Body& body,
                          const std::unordered_set<Vertex*>& vertices,
                          std::size_t face_count, double tolerance) {
  if (face_count <= 6 || vertices.size() < 4) return;

  std::vector<Point3d> positions;
  positions.reserve(vertices.size());
  for (const Vertex* vertex : vertices) {
    if (vertex) positions.push_back(vertex->position());
  }
  if (positions.size() < 4) return;

  const Point3d centroid = average_points(positions);
  double radius = 0.0;
  for (const Point3d& point : positions) radius += (point - centroid).norm();
  radius /= static_cast<double>(positions.size());
  if (!(radius > tolerance)) return;

  const double relative_tolerance = std::max(tolerance, 1e-9);
  const bool common_radius =
      std::all_of(positions.begin(), positions.end(), [&](const Point3d& point) {
        return std::abs((point - centroid).norm() - radius) <=
               relative_tolerance * std::max(1.0, radius);
      });
  if (common_radius) {
    append_candidate(candidates, SnapKind::Center, centroid, body, tolerance);
  }
}

struct LineSegment {
  Edge* edge;
  Body* body;
  Point3d start;
  Point3d end;
};

bool share_vertex(const Edge& first, const Edge& second) {
  return first.v0 == second.v0 || first.v0 == second.v1 ||
         first.v1 == second.v0 || first.v1 == second.v1;
}

}  // namespace

std::vector<SnapCandidate> query_snap_candidates(
    std::span<Body* const> bodies, const SnapQuery& query) {
  std::vector<SnapCandidate> candidates;
  std::vector<LineSegment> line_segments;

  for (Body* body : bodies) {
    if (!body) continue;

    std::unordered_set<Edge*> edges;
    std::unordered_set<Vertex*> vertices;
    std::size_t face_count = 0;

    for (Shell* shell : body->shells) {
      if (!shell) continue;
      for (Face* face : shell->faces) {
        if (!face) continue;
        ++face_count;

        if (kind_enabled(query, SnapKind::Center) && face->surface &&
            face->surface->kind() == SurfaceKind::Sphere) {
          const auto* sphere =
              static_cast<const SphereSurface*>(face->surface);
          append_candidate(candidates, SnapKind::Center, sphere->center(),
                           *body, query.tolerance);
        }

        if (kind_enabled(query, SnapKind::Center) && face->surface &&
            face->surface->kind() == SurfaceKind::Plane) {
          const std::vector<Point3d> positions =
              outer_loop_vertices(*face, query.tolerance);
          if (!positions.empty()) {
            append_candidate(candidates, SnapKind::Center,
                             average_points(positions), *body, query.tolerance);
          }
        }

        for (Loop* loop : face->loops) {
          if (!loop) continue;
          loop->for_each_coedge([&](const CoEdge& coedge) {
            if (coedge.edge) edges.insert(coedge.edge);
          });
        }
      }
    }

    for (Edge* edge : edges) {
      if (!edge) continue;
      if (edge->v0) vertices.insert(edge->v0);
      if (edge->v1) vertices.insert(edge->v1);

      const bool is_line =
          edge->curve && edge->curve->kind() == CurveKind::Line;
      if (is_line) {
        const Point3d start = edge->curve->eval(edge->t0);
        const Point3d end = edge->curve->eval(edge->t1);
        line_segments.push_back({edge, body, start, end});

        if (kind_enabled(query, SnapKind::Perpendicular) &&
            query.reference_point) {
          append_candidate(
              candidates, SnapKind::Perpendicular,
              closest_point_on_segment(*query.reference_point, start, end),
              *body, query.tolerance);
        }
        if (kind_enabled(query, SnapKind::Nearest) && query.near_point) {
          append_candidate(
              candidates, SnapKind::Nearest,
              closest_point_on_segment(*query.near_point, start, end), *body,
              query.tolerance);
        }
      }

      if (kind_enabled(query, SnapKind::Endpoint)) {
        if (edge->v0) {
          append_candidate(candidates, SnapKind::Endpoint,
                           edge->v0->position(), *body, query.tolerance);
        }
        if (edge->v1) {
          append_candidate(candidates, SnapKind::Endpoint,
                           edge->v1->position(), *body, query.tolerance);
        }
      }

      if (kind_enabled(query, SnapKind::Midpoint)) {
        if (edge->curve) {
          append_candidate(candidates, SnapKind::Midpoint,
                           edge->curve->eval(0.5 * (edge->t0 + edge->t1)),
                           *body, query.tolerance);
        } else if (edge->v0 && edge->v1) {
          append_candidate(
              candidates, SnapKind::Midpoint,
              edge->v0->position() +
                  0.5 * (edge->v1->position() - edge->v0->position()),
              *body, query.tolerance);
        }
      }
    }

    if (kind_enabled(query, SnapKind::Center)) {
      append_sphere_center(candidates, *body, vertices, face_count,
                           query.tolerance);
    }
  }

  if (kind_enabled(query, SnapKind::Intersection)) {
    const double tolerance = std::max(0.0, query.tolerance);
    for (std::size_t first_index = 0; first_index < line_segments.size();
         ++first_index) {
      const LineSegment& first = line_segments[first_index];
      for (std::size_t second_index = first_index + 1;
           second_index < line_segments.size(); ++second_index) {
        const LineSegment& second = line_segments[second_index];
        if (first.body == second.body &&
            share_vertex(*first.edge, *second.edge)) {
          continue;
        }

        const SegmentClosestPoints closest = segment_closest_points(
            first.start, first.end, second.start, second.end);
        if (closest.distance > tolerance) continue;

        const Point3d intersection =
            closest.first + 0.5 * (closest.second - closest.first);
        append_candidate(candidates, SnapKind::Intersection, intersection,
                         *first.body, query.tolerance);
        if (first.body != second.body) {
          append_candidate(candidates, SnapKind::Intersection, intersection,
                           *second.body, query.tolerance);
        }
      }
    }
  }

  return candidates;
}

}  // namespace brep
