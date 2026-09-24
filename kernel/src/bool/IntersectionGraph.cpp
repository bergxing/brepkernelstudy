#include "brep/bool/IntersectionGraph.h"

#include "brep/bool/Broadphase.h"
#include "brep/bool/IntersectPlanePlane.h"
#include "brep/bool/IntersectPlaneSphere.h"
#include "brep/bool/IntersectorRegistry.h"
#include "brep/Geometry.h"
#include "brep/internal/Polygon2d.h"
#include "brep/internal/Fuzzy.h"
#include "brep/Topology.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

namespace brep::boolean
{
namespace
{

using brep::internal::PointInPolygon2d;
using brep::internal::PointsEqual;
using brep::internal::SnapTolerance;

[[nodiscard]] std::vector<Point2d> CollectFaceLoopUv(const Face& face)
{
  std::vector<Point2d> polygon;
  if (face.Surface == nullptr || face.Surface->Kind() != SurfaceKind::Plane)
  {
    return polygon;
  }
  const auto& plane = static_cast<const PlaneSurface&>(*face.Surface);
  const Loop* outer = face.OuterLoop();
  if (outer == nullptr || outer->First == nullptr)
  {
    return polygon;
  }
  CoEdge* coedge = outer->First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 128)
    {
      break;
    }
    if (coedge->From() != nullptr)
    {
      polygon.push_back(plane.ParamOf(coedge->From()->Position()));
    }
    coedge = coedge->Next;
  } while (coedge != nullptr && coedge != outer->First);
  return polygon;
}

[[nodiscard]] bool PointInConvexPlanarFace(const Face& face, const Point3d& point,
                                           double eps)
{
  if (!face.Surface || face.Surface->Kind() != SurfaceKind::Plane)
  {
    return false;
  }
  const auto& plane = static_cast<const PlaneSurface&>(*face.Surface);
  if (std::abs(plane.Normal(0.0, 0.0).dot(point - plane.Origin())) > eps)
  {
    return false;
  }
  const std::vector<Point2d> polygon = CollectFaceLoopUv(face);
  if (polygon.size() < 3U)
  {
    return false;
  }
  return PointInPolygon2d(plane.ParamOf(point), polygon, eps);
}

[[nodiscard]] bool IsUnitDirection(const Vector3d& dir, double eps)
{
  return std::abs(dir.norm() - 1.0) <= eps;
}

[[nodiscard]] Point3d EvalLinePoint(const Point3d& origin, const Vector3d& dir,
                                    double t)
{
  return origin + dir * t;
}

[[nodiscard]] Vector3d FaceOutwardNormal(const Face& face)
{
  if (!face.Surface)
  {
    return {};
  }
  Vector3d n = face.Surface->Normal(0.0, 0.0);
  if (face.Sense == Orientation::Reversed)
  {
    n = -n;
  }
  return n.normalized();
}

[[nodiscard]] std::optional<std::pair<Point3d, Point3d>> CoedgeEndPoints(
    const CoEdge& coedge)
{
  if (!coedge.Edge || !coedge.Edge->Curve)
  {
    return std::nullopt;
  }
  if (coedge.Edge->Curve->Kind() != CurveKind::Line)
  {
    return std::nullopt;
  }
  const Point3d a = coedge.Edge->Start(coedge.Sense)->Position();
  const Point3d b = coedge.Edge->End(coedge.Sense)->Position();
  return std::make_pair(a, b);
}

[[nodiscard]] std::pair<Vector3d, Vector3d> CirclePlaneBasis(const Vector3d& normal,
                                                               double eps)
{
  Vector3d u = normal.cross(Vector3d{0, 0, 1});
  if (u.norm() <= eps)
  {
    u = normal.cross(Vector3d{0, 1, 0});
  }
  u = u.normalized();
  const Vector3d v = normal.cross(u).normalized();
  return {u, v};
}

[[nodiscard]] Point3d ProjectToPlaneSurface(const PlaneSurface& plane,
                                            const Point3d& point)
{
  const Vector3d normal = plane.Normal(0.0, 0.0).normalized();
  const double dist = normal.dot(point - plane.Origin());
  return point - normal * dist;
}

[[nodiscard]] Point3d SnapPointToPlanarFaceBoundary(const Face& face,
                                                      const Point3d& point,
                                                      double eps)
{
  Loop* outer = face.OuterLoop();
  if (!outer || !outer->First)
  {
    return point;
  }
  Point3d best = point;
  const double snapEps = SnapTolerance(eps);
  const double snapEpsSq = snapEps * snapEps;
  double bestDistSq = snapEpsSq;
  bool found = false;

  CoEdge* coedge = outer->First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 128)
    {
      break;
    }
    const auto endpoints = CoedgeEndPoints(*coedge);
    if (!endpoints)
    {
      coedge = coedge->Next;
      continue;
    }
    const Point3d& a = endpoints->first;
    const Point3d& b = endpoints->second;
    for (const Point3d* vertex : {&a, &b})
    {
      const double distSq = (*vertex - point).squaredNorm();
      if (distSq <= bestDistSq)
      {
        bestDistSq = distSq;
        best = *vertex;
        found = true;
      }
    }
    const Vector3d ab = b - a;
    const double lenSq = ab.squaredNorm();
    if (lenSq > eps * eps)
    {
      const double t = std::clamp(((point - a).dot(ab)) / lenSq, 0.0, 1.0);
      const Point3d proj = a + ab * t;
      const double distSq = (proj - point).squaredNorm();
      if (distSq <= bestDistSq)
      {
        bestDistSq = distSq;
        best = proj;
        found = true;
      }
    }
    coedge = coedge->Next;
  } while (coedge && coedge != outer->First);

  return found ? best : point;
}

}  // namespace

std::optional<std::pair<double, double>> ClipLineToPlanarFace(
    const Face& face, const Point3d& lineOrigin, const Vector3d& lineDirection,
    double eps)
{
  if (!face.Surface || face.Surface->Kind() != SurfaceKind::Plane)
  {
    return std::nullopt;
  }
  if (!IsUnitDirection(lineDirection, eps))
  {
    return std::nullopt;
  }

  Loop* outer = face.OuterLoop();
  if (!outer || !outer->First)
  {
    return std::nullopt;
  }

  const Vector3d outward = FaceOutwardNormal(face);
  if (outward.norm() <= eps)
  {
    return std::nullopt;
  }

  double tMin = -std::numeric_limits<double>::infinity();
  double tMax = std::numeric_limits<double>::infinity();

  const auto clipEdge = [&](const Point3d& v0, const Point3d& v1) -> bool
  {
    const Vector3d edgeTangent = v1 - v0;
    const double edgeLen = edgeTangent.norm();
    if (edgeLen <= eps)
    {
      return true;
    }
    const Vector3d tangent = edgeTangent / edgeLen;
    const Vector3d inward = outward.cross(tangent).normalized();
    const double denom = inward.dot(lineDirection);
    const double offset = inward.dot(lineOrigin - v0);

    if (std::abs(denom) <= eps)
    {
      if (offset < -eps)
      {
        return false;
      }
      return true;
    }

    const double tBoundary = (-eps - offset) / denom;
    if (denom > 0.0)
    {
      tMin = std::max(tMin, tBoundary);
    }
    else
    {
      tMax = std::min(tMax, tBoundary);
    }
    return true;
  };

  CoEdge* coedge = outer->First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 128)
    {
      return std::nullopt;
    }
    const auto endpoints = CoedgeEndPoints(*coedge);
    if (!endpoints)
    {
      return std::nullopt;
    }
    if (!clipEdge(endpoints->first, endpoints->second))
    {
      return std::nullopt;
    }
    coedge = coedge->Next;
  } while (coedge && coedge != outer->First);

  if (tMin > tMax + eps)
  {
    return std::nullopt;
  }
  return std::make_pair(tMin, tMax);
}

std::vector<IntersectionSegment> IntersectPlanePlaneFaces(const Face& faceA,
                                                            const Face& faceB,
                                                            const BooleanContext& ctx)
{
  std::vector<IntersectionSegment> segments;
  if (!faceA.Surface || !faceB.Surface)
  {
    return segments;
  }
  if (faceA.Surface->Kind() != SurfaceKind::Plane ||
      faceB.Surface->Kind() != SurfaceKind::Plane)
  {
    return segments;
  }

  const double eps = std::max(ctx.fuzzy, 1e-9);
  const auto& planeA = static_cast<const PlaneSurface&>(*faceA.Surface);
  const auto& planeB = static_cast<const PlaneSurface&>(*faceB.Surface);
  const PlanePlaneResult pp = IntersectPlanePlane(planeA, planeB, ctx);
  if (!pp.IsLine())
  {
    return segments;
  }

  Vector3d dir = pp.Direction.normalized();
  if (dir.norm() <= eps)
  {
    return segments;
  }

  const auto clipA =
      ClipLineToPlanarFace(faceA, pp.Point, dir, eps);
  const auto clipB =
      ClipLineToPlanarFace(faceB, pp.Point, dir, eps);
  if (!clipA || !clipB)
  {
    return segments;
  }

  const double tStart = std::max(clipA->first, clipB->first);
  const double tEnd = std::min(clipA->second, clipB->second);
  if (tStart > tEnd + eps)
  {
    return segments;
  }

  IntersectionSegment segment;
  segment.Start = EvalLinePoint(pp.Point, dir, tStart);
  segment.End = EvalLinePoint(pp.Point, dir, tEnd);
  segment.Start = SnapPointToPlanarFaceBoundary(faceA, segment.Start, eps);
  segment.End = SnapPointToPlanarFaceBoundary(faceA, segment.End, eps);
  segment.Start = SnapPointToPlanarFaceBoundary(faceB, segment.Start, eps);
  segment.End = SnapPointToPlanarFaceBoundary(faceB, segment.End, eps);
  segment.Start =
      ProjectToPlaneSurface(planeA, segment.Start);
  segment.End = ProjectToPlaneSurface(planeA, segment.End);
  segment.Start =
      ProjectToPlaneSurface(planeB, segment.Start);
  segment.End = ProjectToPlaneSurface(planeB, segment.End);
  segment.FaceA = const_cast<Face*>(&faceA);
  segment.FaceB = const_cast<Face*>(&faceB);
  segment.CurveKind = CurveKind::Line;
  segments.push_back(segment);
  return segments;
}

[[nodiscard]] Point3d EvalCirclePoint(const Point3d& center,
                                      const Vector3d& axisU,
                                      const Vector3d& axisV, double radius,
                                      double angle)
{
  return center + axisU * (radius * std::cos(angle)) +
         axisV * (radius * std::sin(angle));
}

[[nodiscard]] double CircleAngleAt(const Point3d& center, const Vector3d& axisU,
                                   const Vector3d& axisV, const Point3d& point)
{
  const Vector3d radial = point - center;
  return std::atan2(radial.dot(axisV), radial.dot(axisU));
}

[[nodiscard]] std::vector<Point3d> IntersectCircleWithPlanarBoundary(
    const Face& face, const Point3d& center, double radius, double eps)
{
  std::vector<Point3d> hits;
  const Loop* outer = face.OuterLoop();
  if (outer == nullptr)
  {
    return hits;
  }
  const auto addHit = [&](const Point3d& point)
  {
    for (const Point3d& existing : hits)
    {
      if (PointsEqual(existing, point, eps))
      {
        return;
      }
    }
    hits.push_back(point);
  };

  outer->ForEachCoedge([&](const CoEdge& coedge)
  {
    if (coedge.From() == nullptr || coedge.To() == nullptr)
    {
      return;
    }
    const Point3d a = coedge.From()->Position();
    const Point3d b = coedge.To()->Position();
    if (face.Surface == nullptr)
    {
      return;
    }
    const Vector3d planeNormal = face.Surface->Normal(0.0, 0.0).normalized();
    const double onCircleTol = std::max(eps, 1e-5 * std::max(radius, 1e-9));
    const auto onPlane = [&](const Point3d& point)
    {
      return std::abs(planeNormal.dot(point - center)) <= onCircleTol;
    };
    const auto onCircle = [&](const Point3d& point)
    {
      return onPlane(point) &&
             std::abs((point - center).norm() - radius) <= onCircleTol;
    };
    if (onCircle(a))
    {
      addHit(a);
    }
    if (onCircle(b))
    {
      addHit(b);
    }
    if (coedge.Edge == nullptr || coedge.Edge->Curve == nullptr ||
        coedge.Edge->Curve->Kind() != CurveKind::Line)
    {
      return;
    }
    const Vector3d ab = b - a;
    const double lenSq = ab.squaredNorm();
    if (lenSq <= eps * eps)
    {
      return;
    }
    if (!onPlane(a) || !onPlane(b))
    {
      return;
    }
    const Vector3d ac = a - center;
    const double quadA = lenSq;
    const double quadB = 2.0 * ac.dot(ab);
    const double quadC = ac.squaredNorm() - radius * radius;
    const double disc = quadB * quadB - 4.0 * quadA * quadC;
    if (disc < -eps)
    {
      return;
    }
    const double root = std::sqrt(std::max(0.0, disc));
    const double inv = 0.5 / quadA;
    for (double t : {(-quadB - root) * inv, (-quadB + root) * inv})
    {
      if (t >= -eps && t <= 1.0 + eps)
      {
        addHit(a + ab * std::clamp(t, 0.0, 1.0));
      }
    }
  });
  return hits;
}

void AppendCircleArcSegments(std::vector<IntersectionSegment>& segments,
                             const Face& facePlane, const Face& faceSphere,
                             const Point3d& center, const Vector3d& axisU,
                             const Vector3d& axisV, double radius, double t0,
                             double t1)
{
  constexpr double kTwoPi = 2.0 * std::numbers::pi;
  double span = t1 - t0;
  while (span <= 0.0)
  {
    span += kTwoPi;
  }
  while (span > kTwoPi)
  {
    span -= kTwoPi;
  }
  const int steps = std::max(4, static_cast<int>(std::ceil(32.0 * span / kTwoPi)));
  Point3d prev = EvalCirclePoint(center, axisU, axisV, radius, t0);
  for (int i = 1; i <= steps; ++i)
  {
    const double t =
        t0 + span * (static_cast<double>(i) / static_cast<double>(steps));
    const Point3d next = (i == steps)
                             ? EvalCirclePoint(center, axisU, axisV, radius, t1)
                             : EvalCirclePoint(center, axisU, axisV, radius, t);
    IntersectionSegment segment;
    segment.Start = prev;
    segment.End = next;
    segment.FaceA = const_cast<Face*>(&facePlane);
    segment.FaceB = const_cast<Face*>(&faceSphere);
    segment.CurveKind = CurveKind::Circle;
    segments.push_back(segment);
    prev = next;
  }
}

std::vector<IntersectionSegment> IntersectPlaneSphereFaces(const Face& facePlane,
                                                             const Face& faceSphere,
                                                             const BooleanContext& ctx)
{
  std::vector<IntersectionSegment> segments;
  if (!facePlane.Surface || !faceSphere.Surface)
  {
    return segments;
  }
  if (facePlane.Surface->Kind() != SurfaceKind::Plane ||
      faceSphere.Surface->Kind() != SurfaceKind::Sphere)
  {
    return segments;
  }

  const double eps = std::max(ctx.fuzzy, 1e-9);
  const auto& plane = static_cast<const PlaneSurface&>(*facePlane.Surface);
  const auto& sphere = static_cast<const SphereSurface&>(*faceSphere.Surface);
  const PlaneSphereResult ps = IntersectPlaneSphere(plane, sphere, ctx);
  if (!ps.IsCircle() || !(ps.Radius > eps))
  {
    return segments;
  }

  const Vector3d normal = ps.Normal.normalized();
  const auto [axisU, axisV] = CirclePlaneBasis(normal, eps);
  const Point3d& center = ps.Center;
  const double radius = ps.Radius;
  constexpr double kTwoPi = 2.0 * std::numbers::pi;

  std::vector<Point3d> hits =
      IntersectCircleWithPlanarBoundary(facePlane, center, radius, eps);
  if (hits.size() < 2U)
  {
    const Point3d sample =
        EvalCirclePoint(center, axisU, axisV, radius, 0.0);
    if (hits.empty() && PointInConvexPlanarFace(facePlane, sample, eps))
    {
      AppendCircleArcSegments(segments, facePlane, faceSphere, center, axisU,
                              axisV, radius, 0.0, kTwoPi);
    }
    return segments;
  }

  std::sort(hits.begin(), hits.end(),
            [&](const Point3d& left, const Point3d& right)
            {
              return CircleAngleAt(center, axisU, axisV, left) <
                     CircleAngleAt(center, axisU, axisV, right);
            });

  for (std::size_t i = 0; i < hits.size(); ++i)
  {
    const Point3d& a = hits[i];
    const Point3d& b = hits[(i + 1U) % hits.size()];
    const double t0 = CircleAngleAt(center, axisU, axisV, a);
    double t1 = CircleAngleAt(center, axisU, axisV, b);
    if (t1 <= t0)
    {
      t1 += kTwoPi;
    }
    const double tMid = 0.5 * (t0 + t1);
    const Point3d mid = EvalCirclePoint(center, axisU, axisV, radius, tMid);
    if (!PointInConvexPlanarFace(facePlane, mid, eps))
    {
      continue;
    }
    AppendCircleArcSegments(segments, facePlane, faceSphere, center, axisU,
                            axisV, radius, t0, t1);
  }
  return segments;
}

IntersectionGraph BuildIntersectionGraph(const Body& bodyA, const Body& bodyB,
                                         const BooleanContext& ctx,
                                         spatial::BuildQuality quality)
{
  IntersectorRegistry registry;
  return registry.BuildGraph(bodyA, bodyB, ctx, quality);
}

}  // namespace brep::boolean
