#include "brep/mesh/LoopSample.h"

#include "brep/Geometry.h"
#include "brep/internal/Polygon2d.h"
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
// Only rebuild a circle span when T0/T1 are clearly not the edge vertices
// (boolean weld). Slight vertex snap must keep the stored arc so octant
// trim holes stay on the correct side of the sphere.
constexpr double kParamVertexTol = 1e-3;

[[nodiscard]] double CircleAngle(const CircleCurve& circle, const Point3d& point)
{
  const Vector3d radial = point - circle.Center();
  return std::atan2(radial.dot(circle.YAxis()), radial.dot(circle.XAxis()));
}

[[nodiscard]] double ShortestSignedSpan(double tStart, double tEnd)
{
  constexpr double kPi = std::numbers::pi;
  constexpr double kTwoPi = 2.0 * kPi;
  double span = tEnd - tStart;
  while (span <= -kPi)
  {
    span += kTwoPi;
  }
  while (span > kPi)
  {
    span -= kTwoPi;
  }
  return span;
}

[[nodiscard]] bool CurveParamsMatchVertices(const Edge& edge)
{
  if (edge.Curve == nullptr || edge.V0 == nullptr || edge.V1 == nullptr)
  {
    return false;
  }
  const double tol = std::max(
      {kParamVertexTol, edge.V0->Tolerance, edge.V1->Tolerance});
  return edge.Curve->Eval(edge.T0).distance_to(edge.V0->Position()) <= tol &&
         edge.Curve->Eval(edge.T1).distance_to(edge.V1->Position()) <= tol;
}

[[nodiscard]] bool UvNear(const Point2d& a, const Point2d& b, bool periodicU)
{
  double du = a.u() - b.u();
  if (periodicU)
  {
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    du = std::remainder(du, kTwoPi);
  }
  const double dv = a.v() - b.v();
  return du * du + dv * dv <= 1e-8;
}

[[nodiscard]] std::size_t circle_segment_count(
    const CircleCurve& circle, double parameter_span,
    const TessellationOptions& opts);

[[nodiscard]] std::vector<Point3d> SampleOrientedEdge(
    const Edge& edge, Orientation sense, const TessellationOptions& opts)
{
  if (edge.Curve == nullptr)
  {
    throw std::invalid_argument("SampleEdgeXyz: edge has no curve");
  }

  const Vertex* start = edge.Start(sense);
  const Vertex* end = edge.End(sense);
  const Point3d* snapStart = start != nullptr ? &start->Position() : nullptr;
  const Point3d* snapEnd = end != nullptr ? &end->Position() : nullptr;
  const bool paramsMatch = CurveParamsMatchVertices(edge);

  std::size_t segmentCount = 1;
  double tStart = edge.ParamAt(sense, 0.0);
  double tEnd = edge.ParamAt(sense, 1.0);

  switch (edge.Curve->Kind())
  {
    case CurveKind::Line:
      break;
    case CurveKind::Circle: {
      const auto* circle = dynamic_cast<const CircleCurve*>(edge.Curve);
      if (circle == nullptr)
      {
        throw std::invalid_argument("SampleEdgeXyz: invalid circle curve");
      }
      const auto onCircle = [&](const Point3d* point)
      {
        if (point == nullptr)
        {
          return false;
        }
        const double angle = CircleAngle(*circle, *point);
        return circle->Eval(angle).distance_to(*point) <= kParamVertexTol;
      };
      if (!paramsMatch && onCircle(snapStart) && onCircle(snapEnd))
      {
        tStart = CircleAngle(*circle, *snapStart);
        tEnd = tStart + ShortestSignedSpan(
            tStart, CircleAngle(*circle, *snapEnd));
      }
      segmentCount = circle_segment_count(*circle, tEnd - tStart, opts);
      break;
    }
    case CurveKind::Bezier:
    case CurveKind::Nurbs:
      segmentCount = 32;
      break;
    default:
      throw std::invalid_argument("SampleEdgeXyz: unsupported curve kind");
  }

  std::vector<Point3d> points;
  points.reserve(segmentCount + 1);
  if (edge.Curve->Kind() == CurveKind::Line && !paramsMatch &&
      snapStart != nullptr && snapEnd != nullptr)
  {
    points.push_back(*snapStart);
    points.push_back(*snapEnd);
    return points;
  }

  for (std::size_t i = 0; i <= segmentCount; ++i)
  {
    const double localT =
        static_cast<double>(i) / static_cast<double>(segmentCount);
    points.push_back(edge.Curve->Eval(tStart + (tEnd - tStart) * localT));
  }
  constexpr double kSnapTol = 1e-6;
  if (snapStart != nullptr &&
      points.front().distance_to(*snapStart) <= kSnapTol)
  {
    points.front() = *snapStart;
  }
  if (snapEnd != nullptr &&
      points.back().distance_to(*snapEnd) <= kSnapTol)
  {
    points.back() = *snapEnd;
  }
  return points;
}

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
  std::vector<Point2d> uv;
  uv.reserve(ring.Points.size());
  for (const SampledPoint& sample : ring.Points)
  {
    uv.push_back(sample.Uv);
  }
  return brep::internal::PointInPolygon2d(point, uv);
}

}  // namespace

std::vector<Point3d> SampleEdgeXyz(const Edge& edge,
                                     const TessellationOptions& opts)
{
  return SampleOrientedEdge(edge, Orientation::Forward, opts);
}

std::vector<Point3d> SampleEdgeXyz(const CoEdge& ce,
                                     const TessellationOptions& opts)
{
  if (ce.Edge == nullptr)
  {
    throw std::invalid_argument("SampleEdgeXyz: coedge has no curve");
  }
  return SampleOrientedEdge(*ce.Edge, ce.Sense, opts);
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
    const bool periodicU = surface.Kind() == SurfaceKind::Sphere;
    bool usePcurve = coedge.Pcurve != nullptr &&
                     surface.Kind() != SurfaceKind::Plane;
    if (usePcurve)
    {
      const Point2d p0 = coedge.Pcurve->Eval(0.0);
      const Point2d p1 = coedge.Pcurve->Eval(1.0);
      const Point2d u0 = project_to_surface(surface, edge_points.front());
      const Point2d u1 = project_to_surface(surface, edge_points.back());
      usePcurve = UvNear(p0, u0, periodicU) && UvNear(p1, u1, periodicU);
    }
    for (std::size_t i = 0; i < edge_points.size(); ++i)
    {
      const Point3d& xyz = edge_points[i];
      Point2d uv;
      // Split/copied coedges can retain a pcurve whose parameter range still
      // describes the source edge. Project when the pcurve no longer matches
      // the welded vertices so CDT sees a closed UV polygon.
      if (usePcurve)
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
      // Analytic sphere seam outers live on another UV chart from imprint
      // circles; attach orphan holes to the sole outer region.
      if (surface.Kind() == SurfaceKind::Sphere && regions.size() == 1U)
      {
        regions.front().Holes.push_back(std::move(hole));
        continue;
      }
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
