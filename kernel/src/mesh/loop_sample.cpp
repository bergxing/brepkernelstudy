#include "brep/mesh/loop_sample.hpp"

#include "brep/geometry.hpp"
#include "brep/log.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace brep::mesh {
namespace {

constexpr double kPointTolerance = 1e-12;

[[nodiscard]] std::size_t circle_segment_count(
    const CircleCurve& circle, double parameter_span,
    const TessellationOptions& opts) {
  const double radius = circle.radius();
  if (!(radius > 0.0)) {
    throw std::invalid_argument("sample_edge_xyz: circle radius must be positive");
  }

  const double height =
      opts.linear_deflection > 0.0 ? opts.linear_deflection : 0.02 * radius;
  const double cosine = std::clamp(1.0 - height / radius, -1.0, 1.0);
  const double chord_angle = 2.0 * std::acos(cosine);

  double alpha = chord_angle;
  if (opts.angular_deflection > 0.0) {
    alpha = std::min(alpha, opts.angular_deflection);
  }
  if (!(alpha > 0.0) || !std::isfinite(alpha)) {
    alpha = std::numbers::pi / 180.0;
  }

  const auto required =
      static_cast<std::size_t>(std::ceil(std::abs(parameter_span) / alpha));
  return std::max<std::size_t>(2, required);
}

[[nodiscard]] Point2d project_to_surface(const Surface& surface,
                                         const Point3d& point) {
  if (const auto* plane = dynamic_cast<const PlaneSurface*>(&surface)) {
    return plane->param_of(point);
  }
  if (const auto* sphere = dynamic_cast<const SphereSurface*>(&surface)) {
    Point2d uv = sphere->param_of(point);
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    double u = std::fmod(uv.u(), kTwoPi);
    if (u < 0.0) {
      u += kTwoPi;
    }
    if (u >= kTwoPi) {
      u = 0.0;
    }
    return Point2d{u, uv.v()};
  }
  throw std::invalid_argument(
      "sample_loop: only plane and sphere surfaces are supported");
}

[[nodiscard]] Point2d ring_centroid(const SampledRing& ring) {
  double u = 0.0;
  double v = 0.0;
  for (const SampledPoint& point : ring.points) {
    u += point.uv.u();
    v += point.uv.v();
  }
  const double count = static_cast<double>(ring.points.size());
  return Point2d{u / count, v / count};
}

[[nodiscard]] bool point_in_polygon(const Point2d& point,
                                    const SampledRing& ring) {
  bool inside = false;
  for (std::size_t i = 0, j = ring.points.size() - 1;
       i < ring.points.size(); j = i++) {
    const Point2d& a = ring.points[i].uv;
    const Point2d& b = ring.points[j].uv;
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

std::vector<Point3d> sample_edge_xyz(const CoEdge& ce,
                                     const TessellationOptions& opts) {
  if (!ce.edge || !ce.edge->curve) {
    throw std::invalid_argument("sample_edge_xyz: coedge has no curve");
  }

  const Edge& edge = *ce.edge;
  std::size_t segment_count = 1;
  switch (edge.curve->kind()) {
    case CurveKind::Line:
      break;
    case CurveKind::Circle: {
      const auto* circle = dynamic_cast<const CircleCurve*>(edge.curve);
      if (!circle) {
        throw std::invalid_argument("sample_edge_xyz: invalid circle curve");
      }
      segment_count =
          circle_segment_count(*circle, edge.t1 - edge.t0, opts);
      break;
    }
    default:
      throw std::invalid_argument("sample_edge_xyz: unsupported curve kind");
  }

  std::vector<Point3d> points;
  points.reserve(segment_count + 1);
  for (std::size_t i = 0; i <= segment_count; ++i) {
    const double local_t =
        static_cast<double>(i) / static_cast<double>(segment_count);
    points.push_back(edge.curve->eval(edge.param_at(ce.sense, local_t)));
  }
  return points;
}

SampledRing sample_loop(const Loop& loop, const Surface& surface,
                        const TessellationOptions& opts) {
  SampledRing ring;
  ring.type = loop.type;

  loop.for_each_coedge([&](const CoEdge& coedge) {
    const auto edge_points = sample_edge_xyz(coedge, opts);
    for (std::size_t i = ring.points.empty() ? 0 : 1; i < edge_points.size();
         ++i) {
      const Point3d& xyz = edge_points[i];
      ring.points.push_back({xyz, project_to_surface(surface, xyz)});
    }
  });

  if (ring.points.size() > 1 &&
      ring.points.front().xyz.distance_to(ring.points.back().xyz) <=
          kPointTolerance) {
    ring.points.pop_back();
  }
  return ring;
}

std::vector<FaceRegion> group_face_regions(
    const Face& face, const Surface& surface,
    const TessellationOptions& opts) {
  std::vector<FaceRegion> regions;
  for (const Loop* outer : face.outer_loops()) {
    if (outer) {
      regions.push_back({sample_loop(*outer, surface, opts), {}});
    }
  }

  for (const Loop* inner : face.inner_loops()) {
    if (!inner) {
      continue;
    }
    SampledRing hole = sample_loop(*inner, surface, opts);
    if (hole.points.empty()) {
      BREP_WARN("group_face_regions: empty inner loop on face '{}'", face.name);
      continue;
    }
    const Point2d centroid = ring_centroid(hole);
    const auto region = std::find_if(
        regions.begin(), regions.end(), [&](const FaceRegion& candidate) {
          return candidate.outer.points.size() >= 3 &&
                 point_in_polygon(centroid, candidate.outer);
        });
    if (region == regions.end()) {
      BREP_WARN(
          "group_face_regions: inner loop centroid is outside all outer loops "
          "on face '{}'",
          face.name);
      continue;
    }
    region->holes.push_back(std::move(hole));
  }
  return regions;
}

}  // namespace brep::mesh
