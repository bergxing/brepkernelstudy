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

}  // namespace

std::vector<SnapCandidate> query_snap_candidates(
    std::span<Body* const> bodies, const SnapQuery& query) {
  std::vector<SnapCandidate> candidates;

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

  return candidates;
}

}  // namespace brep
