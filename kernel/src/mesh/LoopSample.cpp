#include "brep/mesh/LoopSample.h"

#include "brep/Geometry.h"
#include "brep/Log.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace brep::mesh
{
namespace
{

constexpr double kPointTolerance = 1e-12;

[[nodiscard]] std::size_t circle_segment_count(
    const CircleCurve& circle, double parameter_span,
    const TessellationOptions& opts)
    {
  const double radius = circle.Radius();
  if (!(radius > 0.0))
  {
    throw std::invalid_argument("SampleEdgeXyz: circle radius must be positive");
  }

  const double height =
      opts.LinearDeflection > 0.0 ? opts.LinearDeflection : 0.02 * radius;
  const double cosine = std::clamp(1.0 - height / radius, -1.0, 1.0);
  const double chord_angle = 2.0 * std::acos(cosine);

  double alpha = chord_angle;
  if (opts.AngularDeflection > 0.0)
  {
    alpha = std::min(alpha, opts.AngularDeflection);
  }
  if (!(alpha > 0.0) || !std::isfinite(alpha))
  {
    alpha = std::numbers::pi / 180.0;
  }

  const auto required =
      static_cast<std::size_t>(std::ceil(std::abs(parameter_span) / alpha));
  return std::max<std::size_t>(24, required);
}

[[nodiscard]] Point2d project_to_surface(const Surface& surface,
                                         const Point3d& point)
{
  if (const auto* plane = dynamic_cast<const PlaneSurface*>(&surface))
{
    return plane->ParamOf(point);
  }
  if (const auto* sphere = dynamic_cast<const SphereSurface*>(&surface))
  {
    Point2d uv = sphere->ParamOf(point);
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    double u = std::fmod(uv.u(), kTwoPi);
    if (u < 0.0)
    {
      u += kTwoPi;
    }
    if (u >= kTwoPi)
    {
      u = 0.0;
    }
    return Point2d{u, uv.v()};
  }
  throw std::invalid_argument(
      "SampleLoop: only plane and sphere surfaces are supported");
}

[[nodiscard]] Point2d ring_centroid(const SampledRing& ring)
{
  double u = 0.0;
  double v = 0.0;
  for (const SampledPoint& point : ring.Points)
  {
    u += point.Uv.u();
    v += point.Uv.v();
  }
  const double count = static_cast<double>(ring.Points.size());
  return Point2d{u / count, v / count};
}

[[nodiscard]] bool point_in_polygon(const Point2d& point,
                                    const SampledRing& ring)
{
  bool inside = false;
  for (std::size_t i = 0, j = ring.Points.size() - 1;
       i < ring.Points.size(); j = i++)
       {
    const Point2d& a = ring.Points[i].Uv;
    const Point2d& b = ring.Points[j].Uv;
    if ((a.v() > point.v()) != (b.v() > point.v()) &&
        point.u() < (b.u() - a.u()) * (point.v() - a.v()) /
                            (b.v() - a.v()) +
                        a.u())
                            {
      inside = !inside;
    }
  }
  return inside;
}

}  // namespace

std::vector<Point3d> SampleEdgeXyz(const CoEdge& ce,
                                     const TessellationOptions& opts)
{
  if (!ce.Edge || !ce.Edge->Curve)
{
    throw std::invalid_argument("SampleEdgeXyz: coedge has no curve");
  }

  const Edge& edge = *ce.Edge;
  std::size_t segment_count = 1;
  switch (edge.Curve->Kind())
  {
    case CurveKind::Line:
      break;
    case CurveKind::Circle: {
      const auto* circle = dynamic_cast<const CircleCurve*>(edge.Curve);
      if (!circle)
      {
        throw std::invalid_argument("SampleEdgeXyz: invalid circle curve");
      }
      segment_count =
          circle_segment_count(*circle, edge.T1 - edge.T0, opts);
      break;
    }
    default:
      throw std::invalid_argument("SampleEdgeXyz: unsupported curve kind");
  }

  std::vector<Point3d> points;
  points.reserve(segment_count + 1);
  for (std::size_t i = 0; i <= segment_count; ++i)
  {
    const double local_t =
        static_cast<double>(i) / static_cast<double>(segment_count);
    points.push_back(edge.Curve->Eval(edge.ParamAt(ce.Sense, local_t)));
  }
  return points;
}

SampledRing SampleLoop(const Loop& loop, const Surface& surface,
                        const TessellationOptions& opts)
{
  SampledRing ring;
  ring.Type = loop.Type;

  loop.ForEachCoedge([&](const CoEdge& coedge)
  {
    const auto edge_points = SampleEdgeXyz(coedge, opts);
    if (edge_points.size() < 2)
    {
      return;
    }
    for (std::size_t i = 0; i < edge_points.size(); ++i)
    {
      const Point3d& xyz = edge_points[i];
      Point2d uv;
      if (coedge.Pcurve)
      {
        const double t = static_cast<double>(i) /
                         static_cast<double>(edge_points.size() - 1);
        uv = coedge.Pcurve->Eval(t);
      }
      else
      {
        uv = project_to_surface(surface, xyz);
      }

      if (!ring.Points.empty() && i == 0)
      {
        const SampledPoint& prev = ring.Points.back();
        const bool same_xyz =
            prev.Xyz.distance_to(xyz) <= kPointTolerance;
        const double du = prev.Uv.u() - uv.u();
        const double dv = prev.Uv.v() - uv.v();
        const bool same_uv = du * du + dv * dv <= 1e-24;
        // Seam partners share 3D but differ in u (0 vs 2π): keep both.
        if (same_xyz && same_uv)
        {
          continue;
        }
      }

      ring.Points.push_back({xyz, uv});
    }
  });

  if (ring.Points.size() > 1)
  {
    const SampledPoint& a = ring.Points.front();
    const SampledPoint& b = ring.Points.back();
    const double du = a.Uv.u() - b.Uv.u();
    const double dv = a.Uv.v() - b.Uv.v();
    if (a.Xyz.distance_to(b.Xyz) <= kPointTolerance &&
        du * du + dv * dv <= 1e-24)
    {
      ring.Points.pop_back();
    }
  }
  return ring;
}

SampledRing UnwrapSphereRing(SampledRing ring)
{
  constexpr double kPi = std::numbers::pi;
  constexpr double kTwoPi = 2.0 * kPi;
  auto& points = ring.Points;
  if (points.size() < 2)
  {
    return ring;
  }

  const auto same_xyz = [](const SampledPoint& a, const SampledPoint& b)
  {
    return a.Xyz.distance_to(b.Xyz) <= kPointTolerance;
  };

  // Sequential unwrap, but do not collapse intentional UV cuts where the same
  // 3D point is identified across the seam (u=0 vs u=2π at poles/seam).
  for (std::size_t i = 1; i < points.size(); ++i)
  {
    if (same_xyz(points[i], points[i - 1]))
  {
      continue;
    }
    double& u = points[i].Uv.u();
    const double prev = points[i - 1].Uv.u();
    while (u - prev > kPi)
    {
      u -= kTwoPi;
    }
    while (u - prev < -kPi)
    {
      u += kTwoPi;
    }
  }

  // Close carefully only when the ring ends are distinct 3D points. Identified
  // seam endpoints already encode the periodic cut and must keep their Δu.
  if (!same_xyz(points.front(), points.back()))
  {
    double& u0 = points.front().Uv.u();
    const double ulast = points.back().Uv.u();
    while (u0 - ulast > kPi)
    {
      u0 -= kTwoPi;
    }
    while (u0 - ulast < -kPi)
    {
      u0 += kTwoPi;
    }
    for (std::size_t i = 1; i < points.size(); ++i)
    {
      if (same_xyz(points[i], points[i - 1]))
    {
        continue;
      }
      double& u = points[i].Uv.u();
      const double prev = points[i - 1].Uv.u();
      while (u - prev > kPi)
      {
        u -= kTwoPi;
      }
      while (u - prev < -kPi)
      {
        u += kTwoPi;
      }
    }
  }

  return ring;
}

std::vector<FaceRegion> GroupFaceRegions(
    const Face& face, const Surface& surface,
    const TessellationOptions& opts)
    {
  std::vector<FaceRegion> regions;
  for (const Loop* outer : face.OuterLoops())
  {
    if (outer)
  {
      regions.push_back({SampleLoop(*outer, surface, opts), {}});
    }
  }

  for (const Loop* inner : face.InnerLoops())
  {
    if (!inner)
  {
      continue;
    }
    SampledRing hole = SampleLoop(*inner, surface, opts);
    if (hole.Points.empty())
    {
      BREP_WARN("GroupFaceRegions: empty inner loop on face '{}'", face.Name);
      continue;
    }
    const Point2d centroid = ring_centroid(hole);
    const auto region = std::find_if(
        regions.begin(), regions.end(), [&](const FaceRegion& candidate)
    {
          return candidate.Outer.Points.size() >= 3 &&
                 point_in_polygon(centroid, candidate.Outer);
        });
    if (region == regions.end())
    {
      BREP_WARN(
          "GroupFaceRegions: inner loop centroid is outside all outer loops "
          "on face '{}'",
          face.Name);
      continue;
    }
    region->Holes.push_back(std::move(hole));
  }
  return regions;
}

}  // namespace brep::mesh
