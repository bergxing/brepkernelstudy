#include "brep/bool/ImprintEngine.h"

#include "brep/bool/BooleanBuilder.h"
#include "brep/bool/Classify.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/bool/SolidClassifier.h"
#include "brep/Geometry.h"
#include "brep/Log.h"
#include "brep/internal/Fuzzy.h"
#include "brep/internal/Polygon2d.h"
#include "brep/Model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace brep::boolean
{
namespace
{

using brep::internal::PointInPolygon2d;
using brep::internal::PointsEqual;
using brep::internal::SnapTolerance;

[[nodiscard]] std::size_t CountBodyEdges(const Body& body)
{
  std::size_t count = 0;
  for (const Shell* shell : body.Shells)
  {
    if (!shell)
    {
      continue;
    }
    for (const Face* face : shell->Faces)
    {
      if (!face)
      {
        continue;
      }
      for (const Loop* loop : face->Loops)
      {
        if (!loop || !loop->First)
        {
          continue;
        }
        CoEdge* coedge = loop->First;
        do
        {
          if (coedge->Edge)
          {
            ++count;
          }
          coedge = coedge->Next;
        } while (coedge && coedge != loop->First);
      }
    }
  }
  return count;
}

[[nodiscard]] std::optional<Point3d> SegmentSegmentIntersect(const Point3d& p0,
                                                               const Point3d& p1,
                                                               const Point3d& q0,
                                                               const Point3d& q1,
                                                               double eps)
{
  const Vector3d d1 = p1 - p0;
  const Vector3d d2 = q1 - q0;
  const Vector3d r = p0 - q0;
  const double a = d1.dot(d1);
  const double e = d2.dot(d2);
  const double b = d1.dot(d2);
  const double c = d1.dot(r);
  const double f = d2.dot(r);
  const double denom = a * e - b * b;
  if (denom <= eps)
  {
    return std::nullopt;
  }
  const double s = (b * f - c * e) / denom;
  const double t = (a * f - b * c) / denom;
  if (s < -eps || s > 1.0 + eps || t < -eps || t > 1.0 + eps)
  {
    return std::nullopt;
  }
  const double sClamped = std::clamp(s, 0.0, 1.0);
  return p0 + d1 * sClamped;
}

[[nodiscard]] double EdgeParamAtPoint(const Edge& edge, const CoEdge& coedge,
                                      const Point3d& point, double eps)
{
  if (!edge.Curve || edge.Curve->Kind() != CurveKind::Line)
  {
    return edge.T0;
  }
  const Point3d a = coedge.From()->Position();
  const Point3d b = coedge.To()->Position();
  const double lenSq = (b - a).squaredNorm();
  if (lenSq <= eps * eps)
  {
    return edge.T0;
  }
  const double local = ((point - a).dot(b - a)) / lenSq;
  const double clamped = std::clamp(local, 0.0, 1.0);
  if (coedge.Sense == Orientation::Forward)
  {
    return edge.T0 + clamped * (edge.T1 - edge.T0);
  }
  return edge.T1 - clamped * (edge.T1 - edge.T0);
}

[[nodiscard]] CoEdge* FindCoedgeFrom(Loop& loop, const Vertex& vertex)
{
  if (!loop.First)
  {
    return nullptr;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 64)
    {
      return nullptr;
    }
    if (coedge->From() == &vertex)
    {
      return coedge;
    }
    coedge = coedge->Next;
  } while (coedge && coedge != loop.First);
  return nullptr;
}

[[nodiscard]] CoEdge* FindCoedgeTo(Loop& loop, const Vertex& vertex)
{
  if (!loop.First)
  {
    return nullptr;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 64)
    {
      return nullptr;
    }
    if (coedge->To() == &vertex)
    {
      return coedge;
    }
    coedge = coedge->Next;
  } while (coedge && coedge != loop.First);
  return nullptr;
}

[[nodiscard]] CoEdge* FindCoedgeContainingPoint(Loop& loop, const Point3d& point,
                                                  double eps)
{
  if (!loop.First)
  {
    return nullptr;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 64)
    {
      return nullptr;
    }
    if (!coedge->Edge || !coedge->Edge->Curve ||
        coedge->Edge->Curve->Kind() != CurveKind::Line)
    {
      coedge = coedge->Next;
      continue;
    }
    const Point3d a = coedge->From()->Position();
    const Point3d b = coedge->To()->Position();
    const Vector3d ab = b - a;
    const double lenSq = ab.squaredNorm();
    if (lenSq <= eps * eps)
    {
      coedge = coedge->Next;
      continue;
    }
    const double t = ((point - a).dot(ab)) / lenSq;
    if (t < -eps || t > 1.0 + eps)
    {
      coedge = coedge->Next;
      continue;
    }
    const Point3d proj = a + ab * std::clamp(t, 0.0, 1.0);
    if (PointsEqual(proj, point, eps))
    {
      return coedge;
    }
    coedge = coedge->Next;
  } while (coedge && coedge != loop.First);
  return nullptr;
}

struct BoundarySnapResult
{
  Point3d Point;
  bool Found{false};
};

[[nodiscard]] BoundarySnapResult SnapToLoopBoundary(const Loop& loop, const Point3d& point,
                                                    double eps)
{
  BoundarySnapResult result;
  result.Point = point;
  if (!loop.First)
  {
    return result;
  }
  const double snapEps = SnapTolerance(eps);
  const double snapEpsSq = snapEps * snapEps;
  double bestDistSq = snapEpsSq;

  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 64)
    {
      break;
    }
    if (!coedge->From() || !coedge->To())
    {
      coedge = coedge->Next;
      continue;
    }
    const Point3d a = coedge->From()->Position();
    const Point3d b = coedge->To()->Position();
    for (const Point3d* vertex : {&a, &b})
    {
      const double distSq = (*vertex - point).squaredNorm();
      if (distSq <= bestDistSq)
      {
        bestDistSq = distSq;
        result.Point = *vertex;
        result.Found = true;
      }
    }
    if (coedge->Edge && coedge->Edge->Curve &&
        coedge->Edge->Curve->Kind() == CurveKind::Line)
    {
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
          result.Point = proj;
          result.Found = true;
        }
      }
    }
    coedge = coedge->Next;
  } while (coedge && coedge != loop.First);

  return result;
}

[[nodiscard]] Point3d ProjectToLoopBoundary(const Loop& loop, const Point3d& point)
{
  if (!loop.First)
  {
    return point;
  }
  Point3d best = point;
  double bestDistSq = std::numeric_limits<double>::infinity();
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 128)
    {
      break;
    }
    if (!coedge->From() || !coedge->To())
    {
      coedge = coedge->Next;
      continue;
    }
    const Point3d a = coedge->From()->Position();
    const Point3d b = coedge->To()->Position();
    for (const Point3d* vertex : {&a, &b})
    {
      const double distSq = (*vertex - point).squaredNorm();
      if (distSq < bestDistSq)
      {
        bestDistSq = distSq;
        best = *vertex;
      }
    }
    if (coedge->Edge && coedge->Edge->Curve &&
        coedge->Edge->Curve->Kind() == CurveKind::Line)
    {
      const Vector3d ab = b - a;
      const double lenSq = ab.squaredNorm();
      if (lenSq > 0.0)
      {
        const double t = std::clamp(((point - a).dot(ab)) / lenSq, 0.0, 1.0);
        const Point3d proj = a + ab * t;
        const double distSq = (proj - point).squaredNorm();
        if (distSq < bestDistSq)
        {
          bestDistSq = distSq;
          best = proj;
        }
      }
    }
    coedge = coedge->Next;
  } while (coedge && coedge != loop.First);
  return best;
}

[[nodiscard]] CoEdge* ResolveCoedgeAtPoint(Loop& loop, const Point3d& point,
                                             double eps)
{
  const BoundarySnapResult snap = SnapToLoopBoundary(loop, point, eps);
  const Point3d snapped = snap.Found ? snap.Point : point;
  if (CoEdge* coedge = FindCoedgeContainingPoint(loop, snapped, eps))
  {
    return coedge;
  }
  if (!loop.First)
  {
    return nullptr;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 64)
    {
      return nullptr;
    }
    if (coedge->From() && PointsEqual(coedge->From()->Position(), snapped, eps))
    {
      return coedge;
    }
    if (coedge->To() && PointsEqual(coedge->To()->Position(), snapped, eps))
    {
      return coedge;
    }
    coedge = coedge->Next;
  } while (coedge && coedge != loop.First);
  return nullptr;
}

[[nodiscard]] bool InsertImprintCoedge(Loop& loop, Vertex& vStart, Vertex& vEnd,
                                       CoEdge& imprintCoedge)
{
  CoEdge* arrive = FindCoedgeTo(loop, vStart);
  CoEdge* depart = FindCoedgeFrom(loop, vEnd);
  if (!arrive || !depart || arrive == depart)
  {
    return false;
  }
  imprintCoedge.Loop = &loop;
  imprintCoedge.Prev = arrive;
  imprintCoedge.Next = depart;
  arrive->Next = &imprintCoedge;
  depart->Prev = &imprintCoedge;
  return true;
}

void CollectBodyFragments(const Body& body, IntersectionGraph& graph)
{
  for (const Shell* shell : body.Shells)
  {
    if (!shell)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (face)
      {
        graph.Fragments.push_back(FaceFragment{face});
      }
    }
  }
}

[[nodiscard]] std::vector<Point2d> CollectFaceLoopUv(const Face& face)
{
  std::vector<Point2d> polygon;
  if (!face.Surface || face.Surface->Kind() != SurfaceKind::Plane)
  {
    return polygon;
  }
  const auto& plane = static_cast<const PlaneSurface&>(*face.Surface);
  const Loop* outer = face.OuterLoop();
  if (!outer || !outer->First)
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
    if (coedge->From())
    {
      polygon.push_back(plane.ParamOf(coedge->From()->Position()));
    }
    coedge = coedge->Next;
  } while (coedge && coedge != outer->First);
  return polygon;
}

[[nodiscard]] double PlanarCoedgeCycleArea(const PlaneSurface& plane,
                                           const std::vector<CoEdge*>& cycle)
{
  std::vector<Point2d> polygon;
  polygon.reserve(cycle.size());
  for (const CoEdge* coedge : cycle)
  {
    if (coedge != nullptr && coedge->From() != nullptr)
    {
      polygon.push_back(plane.ParamOf(coedge->From()->Position()));
    }
  }
  if (polygon.size() < 3U)
  {
    return 0.0;
  }
  double area = 0.0;
  for (std::size_t i = 0; i < polygon.size(); ++i)
  {
    const std::size_t j = (i + 1U) % polygon.size();
    area += polygon[i].u() * polygon[j].v() - polygon[j].u() * polygon[i].v();
  }
  return std::abs(area) * 0.5;
}

[[nodiscard]] bool PointOnOrInPlanarFace(const Face& face, const Point3d& point,
                                         double eps)
{
  if (!face.Surface || face.Surface->Kind() != SurfaceKind::Plane)
  {
    return false;
  }
  const auto& plane = static_cast<const PlaneSurface&>(*face.Surface);
  const double snapEps = SnapTolerance(eps);
  if (std::abs(plane.Normal(0.0, 0.0).dot(point - plane.Origin())) > snapEps)
  {
    return false;
  }
  const std::vector<Point2d> polygon = CollectFaceLoopUv(face);
  if (polygon.empty())
  {
    return false;
  }
  const Point2d uv = plane.ParamOf(point);
  if (PointInPolygon2d(uv, polygon))
  {
    return true;
  }
  Loop* outer = face.OuterLoop();
  if (!outer || !outer->First)
  {
    return false;
  }
  const BoundarySnapResult snap = SnapToLoopBoundary(*outer, point, eps);
  return snap.Found && PointsEqual(snap.Point, point, snapEps);
}

[[nodiscard]] bool PointOnPlanarFace(const Face& face, const Point3d& point, double eps)
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
  if (polygon.empty())
  {
    return false;
  }
  return PointInPolygon2d(plane.ParamOf(point), polygon);
}

[[nodiscard]] std::size_t CountLoopCoedgesSafe(const Loop& loop)
{
  if (!loop.First)
  {
    return 0;
  }
  std::size_t count = 0;
  CoEdge* coedge = loop.First;
  do
  {
    if (++count > 1024U)
    {
      return count;
    }
    coedge = coedge->Next;
  } while (coedge && coedge != loop.First);
  return count;
}

[[nodiscard]] bool HasCircleImprintArc(const Loop& loop, const Vertex& vStart,
                                     const Vertex& vEnd)
{
  CoEdge* fromStart = FindCoedgeFrom(const_cast<Loop&>(loop), vStart);
  if (fromStart != nullptr && fromStart->To() == &vEnd &&
      fromStart->Edge != nullptr && fromStart->Edge->Curve != nullptr &&
      fromStart->Edge->Curve->Kind() == CurveKind::Circle &&
      fromStart->Edge->Name.find("_imprint") != std::string::npos)
  {
    return true;
  }
  CoEdge* fromEnd = FindCoedgeFrom(const_cast<Loop&>(loop), vEnd);
  if (fromEnd != nullptr && fromEnd->To() == &vStart &&
      fromEnd->Edge != nullptr && fromEnd->Edge->Curve != nullptr &&
      fromEnd->Edge->Curve->Kind() == CurveKind::Circle &&
      fromEnd->Edge->Name.find("_imprint") != std::string::npos)
  {
    return true;
  }
  return false;
}

[[nodiscard]] bool HasImprintChord(const Loop& loop, const Vertex& vStart,
                                   const Vertex& vEnd, double eps)
{
  CoEdge* fromStart = FindCoedgeFrom(const_cast<Loop&>(loop), vStart);
  if (fromStart != nullptr && fromStart->To() == &vEnd && fromStart->Edge &&
      fromStart->Edge->Curve &&
      fromStart->Edge->Curve->Kind() == CurveKind::Line)
  {
    return true;
  }
  CoEdge* fromEnd = FindCoedgeFrom(const_cast<Loop&>(loop), vEnd);
  if (fromEnd != nullptr && fromEnd->To() == &vStart && fromEnd->Edge &&
      fromEnd->Edge->Curve &&
      fromEnd->Edge->Curve->Kind() == CurveKind::Line)
  {
    return true;
  }
  (void)eps;
  return false;
}

[[nodiscard]] Face* FindPlanarFaceContainingPoint(Body& body, const Point3d& point,
                                                    double eps)
{
  Face* best = nullptr;
  std::size_t bestCoedgeCount = std::numeric_limits<std::size_t>::max();
  for (Shell* shell : body.Shells)
  {
    if (!shell)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (!face || !PointOnPlanarFace(*face, point, eps))
      {
        continue;
      }
      const Loop* outer = face->OuterLoop();
      const std::size_t coedgeCount =
          outer != nullptr ? CountLoopCoedgesSafe(*outer)
                           : std::numeric_limits<std::size_t>::max();
      if (coedgeCount < bestCoedgeCount)
      {
        bestCoedgeCount = coedgeCount;
        best = face;
      }
    }
  }
  return best;
}

[[nodiscard]] Face* FindPlanarFaceFragment(Shell& shell, const Surface& surface,
                                             const Point3d& point, double eps)
{
  Face* best = nullptr;
  std::size_t bestCoedgeCount = std::numeric_limits<std::size_t>::max();
  for (Face* face : shell.Faces)
  {
    if (face == nullptr || face->Surface != &surface ||
        !PointOnOrInPlanarFace(*face, point, eps))
    {
      continue;
    }
    const Loop* outer = face->OuterLoop();
    const std::size_t coedgeCount =
        outer != nullptr ? CountLoopCoedgesSafe(*outer)
                         : std::numeric_limits<std::size_t>::max();
    if (coedgeCount < bestCoedgeCount)
    {
      bestCoedgeCount = coedgeCount;
      best = face;
    }
  }
  return best;
}

struct QuantizedPointKey
{
  long long x;
  long long y;
  long long z;

  bool operator==(const QuantizedPointKey& other) const = default;
};

struct PlanarImprintKey
{
  const Surface* surface{nullptr};
  QuantizedPointKey a;
  QuantizedPointKey b;

  bool operator<(const PlanarImprintKey& other) const noexcept
  {
    if (surface != other.surface)
    {
      return surface < other.surface;
    }
    if (a.x != other.a.x)
    {
      return a.x < other.a.x;
    }
    if (a.y != other.a.y)
    {
      return a.y < other.a.y;
    }
    if (a.z != other.a.z)
    {
      return a.z < other.a.z;
    }
    if (b.x != other.b.x)
    {
      return b.x < other.b.x;
    }
    if (b.y != other.b.y)
    {
      return b.y < other.b.y;
    }
    return b.z < other.b.z;
  }
};

[[nodiscard]] QuantizedPointKey QuantizePointKey(const Point3d& point, double invEps)
{
  return QuantizedPointKey{static_cast<long long>(std::llround(point.x() * invEps)),
                           static_cast<long long>(std::llround(point.y() * invEps)),
                           static_cast<long long>(std::llround(point.z() * invEps))};
}

[[nodiscard]] PlanarImprintKey MakePlanarImprintKey(const Surface& surface,
                                                    const Point3d& start,
                                                    const Point3d& end, double invEps)
{
  QuantizedPointKey a = QuantizePointKey(start, invEps);
  QuantizedPointKey b = QuantizePointKey(end, invEps);
  if (b.x < a.x || (b.x == a.x && (b.y < a.y || (b.y == a.y && b.z < a.z))))
  {
    std::swap(a, b);
  }
  return PlanarImprintKey{&surface, a, b};
}

struct AnalyticArcSpan
{
    const IntersectionCircle* Circle{nullptr};
    std::vector<Point3d> Points;
    double T0{0.0};
    double T1{0.0};
};

[[nodiscard]] bool HasCircleImprintArcOnFace(const Face& face,
                                             const AnalyticArcSpan& arc,
                                             double eps)
{
  if (arc.Points.size() < 2U)
  {
    return false;
  }
  const Loop* outer = face.OuterLoop();
  if (outer == nullptr || outer->First == nullptr)
  {
    return false;
  }
  CoEdge* coedge = outer->First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 1024U)
    {
      break;
    }
    if (coedge->Edge != nullptr && coedge->Edge->Curve != nullptr &&
        coedge->Edge->Curve->Kind() == CurveKind::Circle &&
        coedge->Edge->Name.find("_imprint") != std::string::npos &&
        coedge->From() != nullptr && coedge->To() != nullptr)
    {
      if ((PointsEqual(coedge->From()->Position(), arc.Points.front(), eps) &&
           PointsEqual(coedge->To()->Position(), arc.Points.back(), eps)) ||
          (PointsEqual(coedge->From()->Position(), arc.Points.back(), eps) &&
           PointsEqual(coedge->To()->Position(), arc.Points.front(), eps)))
      {
        return true;
      }
    }
    coedge = coedge->Next;
  } while (coedge != nullptr && coedge != outer->First);
  return false;
}

[[nodiscard]] bool SamePointOnSphere(const SphereSurface& sphere,
                                     const Point3d& left, const Point3d& right,
                                     double eps)
{
  const double snapEps = SnapTolerance(eps);
  const auto snapOnSphere = [&](const Point3d& point) -> Point3d
  {
    const Vector3d radial = point - sphere.Center();
    if (radial.norm() <= snapEps)
    {
      return point;
    }
    return sphere.Center() + radial.normalized() * sphere.Radius();
  };
  const Point3d snappedLeft = snapOnSphere(left);
  const Point3d snappedRight = snapOnSphere(right);
  if (PointsEqual(snappedLeft, snappedRight, snapEps))
  {
    return true;
  }
  const Vector3d radialLeft = snappedLeft - sphere.Center();
  const Vector3d radialRight = snappedRight - sphere.Center();
  if (radialLeft.norm() <= snapEps || radialRight.norm() <= snapEps)
  {
    return false;
  }
  const double chord = (snappedLeft - snappedRight).norm();
  const double radius = sphere.Radius();
  if (radius > snapEps)
  {
    const double halfAngle =
        std::asin(std::clamp(chord / (2.0 * radius), 0.0, 1.0));
    const double geodesic = 2.0 * halfAngle * radius;
    if (geodesic <= std::max(snapEps, 1e-3 * radius))
    {
      return true;
    }
  }
  return radialLeft.normalized().dot(radialRight.normalized()) >= 1.0 - 1e-5;
}

[[nodiscard]] std::vector<std::vector<AnalyticArcSpan>>
PartitionArcComponents(const std::vector<AnalyticArcSpan>& arcs,
                         const SphereSurface* sphere, double eps)
{
  const double snapEps = SnapTolerance(eps);
  const auto snapOnSphere = [&](const Point3d& point) -> Point3d
  {
    if (sphere == nullptr)
    {
      return point;
    }
    const Vector3d radial = point - sphere->Center();
    if (radial.norm() <= snapEps)
    {
      return point;
    }
    return sphere->Center() + radial.normalized() * sphere->Radius();
  };
  const auto sameOnSphere = [&](const Point3d& left, const Point3d& right) -> bool
  {
    if (sphere != nullptr)
    {
      return SamePointOnSphere(*sphere, left, right, eps);
    }
    return PointsEqual(snapOnSphere(left), snapOnSphere(right), snapEps);
  };
  const std::size_t arcCount = arcs.size();
  if (arcCount == 0U)
  {
    return {};
  }
  const auto shareEndpoint = [&](const AnalyticArcSpan& left,
                                 const AnalyticArcSpan& right) -> bool
  {
    if (left.Points.empty() || right.Points.empty())
    {
      return false;
    }
    const Point3d a0 = left.Points.front();
    const Point3d a1 = left.Points.back();
    const Point3d b0 = right.Points.front();
    const Point3d b1 = right.Points.back();
    return sameOnSphere(a0, b0) || sameOnSphere(a0, b1) || sameOnSphere(a1, b0) ||
           sameOnSphere(a1, b1);
  };

  std::vector<std::vector<std::size_t>> adjacency(arcCount);
  for (std::size_t i = 0; i < arcCount; ++i)
  {
    for (std::size_t j = i + 1U; j < arcCount; ++j)
    {
      if (shareEndpoint(arcs[i], arcs[j]))
      {
        adjacency[i].push_back(j);
        adjacency[j].push_back(i);
      }
    }
  }

  std::vector<bool> visited(arcCount, false);
  std::vector<std::vector<AnalyticArcSpan>> components;
  for (std::size_t i = 0; i < arcCount; ++i)
  {
    if (visited[i])
    {
      continue;
    }
    std::vector<AnalyticArcSpan> component;
    std::vector<std::size_t> stack{i};
    visited[i] = true;
    while (!stack.empty())
    {
      const std::size_t index = stack.back();
      stack.pop_back();
      component.push_back(arcs[index]);
      for (const std::size_t neighbor : adjacency[index])
      {
        if (!visited[neighbor])
        {
          visited[neighbor] = true;
          stack.push_back(neighbor);
        }
      }
    }
    components.push_back(std::move(component));
  }
  return components;
}

[[nodiscard]] double NormalizeAngle(double angle)
{
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    angle = std::fmod(angle, kTwoPi);
    return angle < 0.0 ? angle + kTwoPi : angle;
}

[[nodiscard]] double CircleParamAt(const CircleCurve& curve,
                                   const Point3d& point)
{
    const Vector3d xAxis =
        (curve.Eval(0.0) - curve.Center()).normalized();
    const Vector3d yAxis =
        (curve.Eval(0.5 * std::numbers::pi) - curve.Center()).normalized();
    const Vector3d radial = point - curve.Center();
    return NormalizeAngle(
        std::atan2(radial.dot(yAxis), radial.dot(xAxis)));
}

[[nodiscard]] std::vector<AnalyticArcSpan> CollectAnalyticArcSpans(
    const IntersectionGraph& graph, const IntersectionCircle& circle,
    double eps)
{
    std::vector<std::vector<Point3d>> runs;
    for (const IntersectionSegment& segment : graph.Segments)
    {
        if (segment.CurveKind != CurveKind::Circle ||
            segment.FaceA != circle.FaceA || segment.FaceB != circle.FaceB)
        {
            continue;
        }
        if (runs.empty() ||
            !PointsEqual(runs.back().back(), segment.Start, eps))
        {
            runs.push_back({segment.Start, segment.End});
        }
        else
        {
            runs.back().push_back(segment.End);
        }
    }
    if (runs.size() > 1U &&
        PointsEqual(runs.back().back(), runs.front().front(), eps))
    {
        std::vector<Point3d> merged = std::move(runs.back());
        runs.pop_back();
        merged.insert(merged.end(), std::next(runs.front().begin()),
                      runs.front().end());
        runs.front() = std::move(merged);
    }

    std::vector<AnalyticArcSpan> spans;
    CircleCurve support(circle.Center, circle.Normal, circle.Radius);
    for (std::vector<Point3d>& points : runs)
    {
        if (points.size() < 2U)
        {
            continue;
        }
        if (PointsEqual(points.front(), points.back(), eps))
        {
            if (points.size() > 1U)
            {
                points.pop_back();
            }
            spans.push_back(AnalyticArcSpan{
                &circle, std::move(points), 0.0, 2.0 * std::numbers::pi});
            continue;
        }
        double sampledAngle = 0.0;
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            const Vector3d previous =
                (points[i - 1U] - circle.Center).normalized();
            const Vector3d current =
                (points[i] - circle.Center).normalized();
            sampledAngle += std::acos(
                std::clamp(previous.dot(current), -1.0, 1.0));
        }
        double t0 = CircleParamAt(support, points.front());
        double t1 = CircleParamAt(support, points.back());
        double forward = NormalizeAngle(t1 - t0);
        if (std::abs(forward - sampledAngle) >
            std::abs((2.0 * std::numbers::pi - forward) - sampledAngle))
        {
            std::reverse(points.begin(), points.end());
            t0 = CircleParamAt(support, points.front());
            t1 = CircleParamAt(support, points.back());
            forward = NormalizeAngle(t1 - t0);
        }
        spans.push_back(
            AnalyticArcSpan{&circle, std::move(points), t0, t0 + forward});
    }
    return spans;
}

[[nodiscard]] Curve2d* MakeProjectedPcurve(
    Model& model, const Surface& surface, std::vector<Point3d> points,
    bool reverse)
{
    if (reverse)
    {
        std::reverse(points.begin(), points.end());
    }
    std::vector<Point2d> parameters;
    parameters.reserve(points.size());
    if (surface.Kind() == SurfaceKind::Sphere)
    {
        const auto& sphere = static_cast<const SphereSurface&>(surface);
        for (const Point3d& point : points)
        {
            parameters.push_back(sphere.ParamOf(point));
        }
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const Vector3d radial = points[i] - sphere.Center();
            if (std::hypot(radial.x(), radial.z()) <=
                1e-10 * sphere.Radius())
            {
                const std::size_t neighbor =
                    i > 0U ? i - 1U : std::min<std::size_t>(1U, points.size() - 1U);
                parameters[i] =
                    Point2d{parameters[neighbor].u(), parameters[i].v()};
            }
        }
        for (std::size_t i = 1; i < parameters.size(); ++i)
        {
            double u = parameters[i].u();
            const double previous = parameters[i - 1U].u();
            while (u - previous > std::numbers::pi)
            {
                u -= 2.0 * std::numbers::pi;
            }
            while (u - previous < -std::numbers::pi)
            {
                u += 2.0 * std::numbers::pi;
            }
            parameters[i] = Point2d{u, parameters[i].v()};
        }
    }
    else if (surface.Kind() == SurfaceKind::Plane)
    {
        const auto& plane = static_cast<const PlaneSurface&>(surface);
        for (const Point3d& point : points)
        {
            parameters.push_back(plane.ParamOf(point));
        }
    }
    else
    {
        return nullptr;
    }
    return model.MakePolyline2d(std::move(parameters));
}

[[nodiscard]] Face* ResolveImprintFace(Shell& shell, const Surface& surface,
                                       const Point3d& start, const Point3d& end,
                                       double eps)
{
  const Point3d mid = start + (end - start) * 0.5;
  for (const Point3d* probe : {&mid, &start, &end})
  {
    if (Face* face = FindPlanarFaceFragment(shell, surface, *probe, eps))
    {
      return face;
    }
  }
  return nullptr;
}

[[nodiscard]] Face* FindPlanarFaceFragment(Body& body, const Surface& surface,
                                             const Point3d& point, double eps)
{
  for (Shell* shell : body.Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    if (Face* face = FindPlanarFaceFragment(*shell, surface, point, eps))
    {
      return face;
    }
  }
  return nullptr;
}

[[nodiscard]] CoEdge* FindCoedgeDeparting(Loop& loop, const Vertex& vertex)
{
  if (CoEdge* coedge = FindCoedgeFrom(loop, vertex))
  {
    return coedge;
  }
  if (CoEdge* arrive = FindCoedgeTo(loop, vertex))
  {
    return arrive->Next;
  }
  return nullptr;
}

[[nodiscard]] std::vector<CoEdge*> CollectLoopPath(Loop& loop, const Vertex& from,
                                                     const Vertex& to)
{
  std::vector<CoEdge*> path;
  CoEdge* start = FindCoedgeDeparting(loop, from);
  if (start == nullptr)
  {
    return path;
  }
  CoEdge* cur = start;
  std::size_t guard = 0;
  do
  {
    if (++guard > 128)
    {
      return {};
    }
    path.push_back(cur);
    if (cur->To() == &to)
    {
      return path;
    }
    cur = cur->Next;
  } while (cur != nullptr && cur != start);
  return {};
}

[[nodiscard]] std::vector<CoEdge*> CollectLoopPathReverse(Loop& loop,
                                                            const Vertex& from,
                                                            const Vertex& to)
{
  std::vector<CoEdge*> path;
  CoEdge* start = FindCoedgeDeparting(loop, from);
  if (start == nullptr)
  {
    return path;
  }
  CoEdge* cur = start;
  std::size_t guard = 0;
  do
  {
    if (++guard > 128)
    {
      return {};
    }
    path.push_back(cur);
    if (cur->To() == &to)
    {
      return path;
    }
    cur = cur->Prev;
  } while (cur != nullptr && cur != start);
  return {};
}

[[nodiscard]] bool PathLiesOnSegment(const std::vector<CoEdge*>& path,
                                     const Vertex& start, const Vertex& end,
                                     double eps)
{
  if (path.empty())
  {
    return false;
  }
  const double snapEps = SnapTolerance(eps);
  const Point3d a = start.Position();
  const Point3d b = end.Position();
  const Vector3d ab = b - a;
  const double abLen2 = ab.squaredNorm();
  if (abLen2 <= snapEps * snapEps)
  {
    return false;
  }
  const auto onSegment = [&](const Point3d& point)
  {
    const double t = (point - a).dot(ab) / abLen2;
    if (t < -1e-6 || t > 1.0 + 1e-6)
    {
      return false;
    }
    const Point3d projected = a + ab * std::clamp(t, 0.0, 1.0);
    return (point - projected).norm() <= snapEps;
  };
  for (const CoEdge* coedge : path)
  {
    if (coedge == nullptr || coedge->From() == nullptr || coedge->To() == nullptr)
    {
      return false;
    }
    if (!onSegment(coedge->From()->Position()) ||
        !onSegment(coedge->To()->Position()))
    {
      return false;
    }
  }
  return true;
}

bool SplitFaceAtChord(Model& model, Shell& shell, Face& face, Loop& outer,
                      Vertex& vStart, Vertex& vEnd)
{
  if (&vStart == &vEnd)
  {
    return false;
  }
  std::vector<CoEdge*> forwardPath = CollectLoopPath(outer, vStart, vEnd);
  std::vector<CoEdge*> backwardPath = CollectLoopPath(outer, vEnd, vStart);
  if (forwardPath.empty() || backwardPath.empty())
  {
    return false;
  }
  if (!face.Surface || face.Surface->Kind() != SurfaceKind::Plane)
  {
    return false;
  }
  auto& plane = static_cast<PlaneSurface&>(*face.Surface);

  if (forwardPath.size() + backwardPath.size() < 3U)
  {
    return false;
  }

  LineCurve* imprintCurve = model.MakeLine(vStart.Position(), vEnd.Position());
  Edge* imprintEdge = model.MakeEdge(imprintCurve, &vStart, &vEnd, 0.0,
                                     imprintCurve->Length(), face.Tolerance,
                                     face.Name + "_imprint");
  Curve2d* pcurveFwd =
      model.MakeLine2d(plane.ParamOf(vStart.Position()), plane.ParamOf(vEnd.Position()));
  Curve2d* pcurveRev =
      model.MakeLine2d(plane.ParamOf(vEnd.Position()), plane.ParamOf(vStart.Position()));
  CoEdge* ceFwd = model.MakeCoedge(imprintEdge, Orientation::Forward, pcurveFwd,
                                   face.Name + "_imprint_fwd");
  CoEdge* ceRev = model.MakeCoedge(imprintEdge, Orientation::Reversed, pcurveRev,
                                   face.Name + "_imprint_rev");
  Model::PairPartners(ceFwd, ceRev);

  Face* newFace = model.MakeFace(face.Surface, face.Sense, face.Name + "_split");
  shell.Faces.push_back(newFace);
  Loop* newLoop = model.MakeLoop(newFace, LoopType::Outer, newFace->Name + "_outer");

  std::vector<CoEdge*> outerCycle;
  outerCycle.reserve(forwardPath.size() + 1U);
  for (CoEdge* coedge : forwardPath)
  {
    coedge->Loop = &outer;
    outerCycle.push_back(coedge);
  }
  ceRev->Loop = &outer;
  outerCycle.push_back(ceRev);
  Model::LinkLoop(&outer, outerCycle);

  std::vector<CoEdge*> innerCycle;
  innerCycle.reserve(backwardPath.size() + 1U);
  ceFwd->Loop = newLoop;
  innerCycle.push_back(ceFwd);
  for (CoEdge* coedge : backwardPath)
  {
    coedge->Loop = newLoop;
    innerCycle.push_back(coedge);
  }
  Model::LinkLoop(newLoop, innerCycle);

  return true;
}

[[nodiscard]] Point3d CycleSamplePoint(const std::vector<CoEdge*>& cycle)
{
  Vector3d sum;
  std::size_t count = 0;
  for (CoEdge* coedge : cycle)
  {
    if (coedge == nullptr || coedge->Edge == nullptr ||
        coedge->Edge->Curve == nullptr)
    {
      continue;
    }
    const Point3d mid = coedge->Edge->Curve->Eval(
        0.5 * (coedge->Edge->T0 + coedge->Edge->T1));
    sum += Vector3d{mid.x(), mid.y(), mid.z()};
    ++count;
  }
  if (count == 0)
  {
    return {};
  }
  const Vector3d avg = sum / static_cast<double>(count);
  return Point3d{avg.x(), avg.y(), avg.z()};
}

[[nodiscard]] bool WedgeSampleInsideOther(const Body& other, const Point3d& p,
                                          double eps)
{
  return ClassifyPointInBody(other, p, eps) == SolidClass::In;
}

[[nodiscard]] Vertex* FindVertexOnShellAtPoint(Shell& shell, const Point3d& point,
                                               double eps)
{
  const double snapEps = SnapTolerance(eps);
  for (Face* face : shell.Faces)
  {
    if (face == nullptr)
    {
      continue;
    }
    for (Loop* loop : face->Loops)
    {
      if (loop == nullptr)
      {
        continue;
      }
      Vertex* found = nullptr;
      loop->ForEachCoedge([&](CoEdge& coedge)
      {
        if (found != nullptr)
        {
          return;
        }
        if (coedge.From() != nullptr &&
            PointsEqual(coedge.From()->Position(), point, snapEps))
        {
          found = coedge.From();
        }
        else if (coedge.To() != nullptr &&
                 PointsEqual(coedge.To()->Position(), point, snapEps))
        {
          found = coedge.To();
        }
      });
      if (found != nullptr)
      {
        return found;
      }
    }
  }
  return nullptr;
}

[[nodiscard]] Point3d WedgeInteriorSample(const Face& face,
                                          const std::vector<CoEdge*>& cycle,
                                          const AnalyticArcSpan& arc)
{
  if (arc.Points.empty())
  {
    return CycleSamplePoint(cycle);
  }
  const Point3d arcMid =
      arc.Points.size() == 1U
          ? arc.Points.front()
          : arc.Points[arc.Points.size() / 2U];
  const Point3d centroid = CycleSamplePoint(cycle);
  Vector3d offset = centroid - arcMid;
  if (offset.norm() <= 1e-9 && face.Surface != nullptr &&
      face.Surface->Kind() == SurfaceKind::Plane && arc.Points.size() >= 2U)
  {
    const auto& plane = static_cast<const PlaneSurface&>(*face.Surface);
    const Point3d tangentPoint = arc.Points.back();
    offset = plane.Normal(0.0, 0.0).cross(tangentPoint - arcMid);
  }
  if (offset.norm() <= 1e-9)
  {
    return arcMid;
  }
  return arcMid + offset.normalized() * 1e-4;
}

[[nodiscard]] Vertex* EnsureBoundaryVertex(Model& model, Shell& shell,
                                           Loop& loop, const Point3d& point,
                                           double eps)
{
    const double snapEps = SnapTolerance(eps);
    if (Vertex* shellVertex = FindVertexOnShellAtPoint(shell, point, eps))
    {
        return shellVertex;
    }
    Vertex* existing = nullptr;
    loop.ForEachCoedge([&](CoEdge& coedge)
    {
        if (existing != nullptr)
        {
            return;
        }
        if (coedge.From() != nullptr &&
            PointsEqual(coedge.From()->Position(), point, snapEps))
        {
            existing = coedge.From();
        }
        else if (coedge.To() != nullptr &&
                 PointsEqual(coedge.To()->Position(), point, snapEps))
        {
            existing = coedge.To();
        }
    });
    if (existing != nullptr)
    {
        return existing;
    }
    CoEdge* coedge = ResolveCoedgeAtPoint(loop, point, eps);
    if (coedge == nullptr || coedge->Edge == nullptr)
    {
        return nullptr;
    }
    if (PointsEqual(coedge->From()->Position(), point, eps))
    {
        return coedge->From();
    }
    if (PointsEqual(coedge->To()->Position(), point, eps))
    {
        return coedge->To();
    }
    const double parameter =
        EdgeParamAtPoint(*coedge->Edge, *coedge, point, eps);
    const SplitEdgeResult split =
        SplitEdgeAt(model, *coedge->Edge, parameter, eps);
    return split.Ok ? split.SplitVertex : nullptr;
}

[[nodiscard]] bool SplitPlanarFaceAtArc(Model& model, Shell& shell,
                                        Face& face,
                                        const AnalyticArcSpan& arc,
                                        double eps, const Body* bodyA,
                                        const Body* bodyB, BooleanOp op)
{
    if (face.Surface == nullptr ||
        face.Surface->Kind() != SurfaceKind::Plane ||
        arc.Circle == nullptr)
    {
        return false;
    }
    Loop* outer = face.OuterLoop();
    if (outer == nullptr)
    {
        return false;
    }
    Vertex* start = EnsureBoundaryVertex(
        model, shell, *outer, ProjectToLoopBoundary(*outer, arc.Points.front()),
        eps);
    Vertex* end = EnsureBoundaryVertex(
        model, shell, *outer, ProjectToLoopBoundary(*outer, arc.Points.back()),
        eps);
    if (start == nullptr || end == nullptr)
    {
        return false;
    }
    if (start == end)
    {
        return true;
    }
    if (HasCircleImprintArc(*outer, *start, *end))
    {
        return true;
    }
    std::vector<CoEdge*> forwardPath =
        CollectLoopPath(*outer, *start, *end);
    if (forwardPath.empty())
    {
        forwardPath = CollectLoopPathReverse(*outer, *start, *end);
    }
    std::vector<CoEdge*> backwardPath =
        CollectLoopPath(*outer, *end, *start);
    if (backwardPath.empty())
    {
        backwardPath = CollectLoopPathReverse(*outer, *end, *start);
    }
    if (forwardPath.empty() || backwardPath.empty())
    {
        return false;
    }

    const std::string name = face.Name + "_circle_imprint";
    CircleCurve* curve =
        model.MakeCircle(arc.Circle->Center, arc.Circle->Normal,
                         arc.Circle->Radius, name + "_curve");
    Edge* edge = model.MakeEdge(curve, start, end, arc.T0, arc.T1,
                                face.Tolerance, name + "_edge");
    CoEdge* forward = model.MakeCoedge(
        edge, Orientation::Forward,
        MakeProjectedPcurve(model, *face.Surface, arc.Points, false),
        name + "_forward");
    CoEdge* reversed = model.MakeCoedge(
        edge, Orientation::Reversed,
        MakeProjectedPcurve(model, *face.Surface, arc.Points, true),
        name + "_reversed");
    Model::PairPartners(forward, reversed);

    Face* split =
        model.MakeFace(face.Surface, face.Sense, face.Name + "_split");
    shell.Faces.push_back(split);
    Loop* splitOuter =
        model.MakeLoop(split, LoopType::Outer, split->Name + "_outer");

    std::vector<CoEdge*> splitCycleA;
    splitCycleA.push_back(forward);
    splitCycleA.insert(splitCycleA.end(), backwardPath.begin(), backwardPath.end());

    std::vector<CoEdge*> splitCycleB;
    splitCycleB.insert(splitCycleB.end(), forwardPath.begin(), forwardPath.end());
    splitCycleB.push_back(reversed);

    const auto countExteriorBoundary =
        [](const std::vector<CoEdge*>& cycle) -> std::size_t
    {
      std::size_t count = 0;
      for (CoEdge* coedge : cycle)
      {
        if (coedge == nullptr || coedge->Edge == nullptr)
        {
          continue;
        }
        const std::string& edgeName = coedge->Edge->Name;
        if (edgeName.find("_outer_te") != std::string::npos ||
            edgeName.find("_outer_be") != std::string::npos ||
            edgeName.find("_outer_ve") != std::string::npos)
        {
          ++count;
        }
      }
      return count;
    };

    const auto countUnselectedPartners =
        [&face](const std::vector<CoEdge*>& cycle) -> std::size_t
    {
      std::size_t count = 0;
      for (CoEdge* coedge : cycle)
      {
        if (coedge == nullptr || coedge->Partner == nullptr)
        {
          continue;
        }
        const Face* partnerFace = coedge->Partner->GetFace();
        if (partnerFace == nullptr || partnerFace == &face)
        {
          continue;
        }
        if (partnerFace->Name.find("_split") != std::string::npos ||
            partnerFace->Name.find("_sphere_imprint_patch") != std::string::npos)
        {
          continue;
        }
        ++count;
      }
      return count;
    };

    const auto pickSplitCycle =
        [&](const std::vector<CoEdge*>& cycleA,
            const std::vector<CoEdge*>& cycleB) -> const std::vector<CoEdge*>*
    {
      const std::size_t exteriorA = countExteriorBoundary(cycleA);
      const std::size_t exteriorB = countExteriorBoundary(cycleB);
      if (exteriorA != exteriorB)
      {
        return exteriorA < exteriorB ? &cycleA : &cycleB;
      }
      const std::size_t partnersA = countUnselectedPartners(cycleA);
      const std::size_t partnersB = countUnselectedPartners(cycleB);
      if (partnersA != partnersB)
      {
        return partnersA < partnersB ? &cycleA : &cycleB;
      }
      const auto bodyOwnsFace = [](const Body& body, const Face& owned) -> bool
      {
        for (const Shell* shell : body.Shells)
        {
          if (shell == nullptr)
          {
            continue;
          }
          if (std::find(shell->Faces.begin(), shell->Faces.end(), &owned) !=
              shell->Faces.end())
          {
            return true;
          }
        }
        return false;
      };
      const Body* other = nullptr;
      if (bodyA != nullptr && bodyOwnsFace(*bodyA, face))
      {
        other = bodyB;
      }
      else if (bodyB != nullptr)
      {
        other = bodyA;
      }
      if (other != nullptr)
      {
        const Point3d sampleA = WedgeInteriorSample(face, cycleA, arc);
        const Point3d sampleB = WedgeInteriorSample(face, cycleB, arc);
        const bool insideA = WedgeSampleInsideOther(*other, sampleA, eps);
        const bool insideB = WedgeSampleInsideOther(*other, sampleB, eps);
        if (insideA != insideB)
        {
          return insideA ? &cycleA : &cycleB;
        }
      }
      (void)op;
      return cycleA.size() <= cycleB.size() ? &cycleB : &cycleA;
    };

    std::vector<CoEdge*> complementCycle;
    const std::vector<CoEdge*>* splitCycle = pickSplitCycle(splitCycleA, splitCycleB);
    if (splitCycle == &splitCycleA)
    {
      complementCycle.insert(complementCycle.end(), splitCycleB.begin(),
                             splitCycleB.end());
    }
    else
    {
      complementCycle = splitCycleA;
    }

    Model::LinkLoop(outer, complementCycle);
    Model::LinkLoop(splitOuter, *splitCycle);
    return true;
}

[[nodiscard]] bool ImprintArcCycleOnSphereFace(
    Model& model, Shell& shell, Face& face,
    const std::vector<AnalyticArcSpan>& arcs, double eps)
{
    if (face.Surface == nullptr ||
        face.Surface->Kind() != SurfaceKind::Sphere || arcs.size() < 2U)
    {
        return false;
    }

    struct ArcEdge
    {
        Edge* EdgeValue{nullptr};
        Vertex* Start{nullptr};
        Vertex* End{nullptr};
        const AnalyticArcSpan* Span{nullptr};
    };
    const auto& sphereSurface =
        static_cast<const SphereSurface&>(*face.Surface);
    const double snapEps = SnapTolerance(eps);
    const auto snapToSphere = [&](const Point3d& point) -> Point3d
    {
        const Vector3d radial = point - sphereSurface.Center();
        if (radial.norm() <= snapEps)
        {
            return point;
        }
        return sphereSurface.Center() +
               radial.normalized() * sphereSurface.Radius();
    };
    std::vector<std::pair<Point3d, Vertex*>> vertices;
    const auto vertexFor = [&](const Point3d& point) -> Vertex*
    {
        const Point3d snapped = snapToSphere(point);
        for (const auto& [position, vertex] : vertices)
        {
            if (SamePointOnSphere(sphereSurface, position, snapped, eps))
            {
                return vertex;
            }
        }
        Vertex* vertex = model.MakeVertex(
            model.MakePoint(snapped, face.Name + "_sphere_imprint_point"), eps,
            face.Name + "_sphere_imprint_vertex");
        vertices.emplace_back(snapped, vertex);
        return vertex;
    };

    std::vector<ArcEdge> edges;
    edges.reserve(arcs.size());
    for (const AnalyticArcSpan& arc : arcs)
    {
        if (arc.Circle == nullptr)
        {
            return false;
        }
        Point3d startPoint = snapToSphere(arc.Points.front());
        Point3d endPoint = snapToSphere(arc.Points.back());
        Vertex* start = vertexFor(startPoint);
        Vertex* end = vertexFor(endPoint);
        CircleCurve* curve = model.MakeCircle(
            arc.Circle->Center, arc.Circle->Normal, arc.Circle->Radius,
            face.Name + "_sphere_imprint_curve");
        Edge* edge = model.MakeEdge(
            curve, start, end, arc.T0, arc.T1, face.Tolerance,
            face.Name + "_sphere_imprint_edge");
        edges.push_back(ArcEdge{edge, start, end, &arc});
    }

    const auto sameVertex = [&](const Vertex* left, const Vertex* right) -> bool
    {
        if (left == nullptr || right == nullptr)
        {
            return false;
        }
        if (left == right)
        {
            return true;
        }
        return SamePointOnSphere(sphereSurface, left->Position(), right->Position(),
                                 eps);
    };

    const auto tryChainFrom = [&](std::size_t startIndex, bool forwardFirst)
        -> std::optional<std::vector<std::pair<ArcEdge*, Orientation>>>
    {
        std::vector<bool> localUsed(edges.size(), false);
        std::vector<std::pair<ArcEdge*, Orientation>> localOrder;
        localOrder.reserve(edges.size());
        localUsed[startIndex] = true;
        if (forwardFirst)
        {
            localOrder.emplace_back(&edges[startIndex], Orientation::Forward);
        }
        else
        {
            localOrder.emplace_back(&edges[startIndex], Orientation::Reversed);
        }
        Vertex* cycleStartVertex =
            forwardFirst ? edges[startIndex].Start : edges[startIndex].End;
        Vertex* currentVertex =
            forwardFirst ? edges[startIndex].End : edges[startIndex].Start;
        while (localOrder.size() < edges.size())
        {
            bool found = false;
            for (std::size_t i = 0; i < edges.size(); ++i)
            {
                if (localUsed[i])
                {
                    continue;
                }
                if (sameVertex(edges[i].Start, currentVertex))
                {
                    localOrder.emplace_back(&edges[i], Orientation::Forward);
                    currentVertex = edges[i].End;
                    localUsed[i] = true;
                    found = true;
                    break;
                }
                if (sameVertex(edges[i].End, currentVertex))
                {
                    localOrder.emplace_back(&edges[i], Orientation::Reversed);
                    currentVertex = edges[i].Start;
                    localUsed[i] = true;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return std::nullopt;
            }
        }
        if (!sameVertex(currentVertex, cycleStartVertex))
        {
            return std::nullopt;
        }
        return localOrder;
    };

    std::optional<std::vector<std::pair<ArcEdge*, Orientation>>> ordered;
    for (std::size_t startIndex = 0; startIndex < edges.size() && !ordered.has_value();
         ++startIndex)
    {
        ordered = tryChainFrom(startIndex, true);
        if (!ordered.has_value())
        {
            ordered = tryChainFrom(startIndex, false);
        }
    }
    if (!ordered.has_value())
    {
        return false;
    }

    const std::string name = face.Name + "_sphere_imprint";
    Face* patch = model.MakeFace(face.Surface, face.Sense, name + "_patch");
    shell.Faces.push_back(patch);
    Loop* patchOuter =
        model.MakeLoop(patch, LoopType::Outer, name + "_patch_outer");
    Loop* complementInner =
        model.MakeLoop(&face, LoopType::Inner, name + "_complement_inner");

    std::vector<CoEdge*> patchUses;
    std::vector<CoEdge*> complementUses;
    patchUses.reserve(ordered->size());
    complementUses.reserve(ordered->size());
    for (const auto& [arc, sense] : *ordered)
    {
        patchUses.push_back(model.MakeCoedge(
            arc->EdgeValue, sense,
            MakeProjectedPcurve(model, *face.Surface, arc->Span->Points,
                                sense == Orientation::Reversed),
            name + "_patch_coedge"));
    }
    for (auto it = ordered->rbegin(); it != ordered->rend(); ++it)
    {
        const Orientation reverseSense = opposite(it->second);
        complementUses.push_back(model.MakeCoedge(
            it->first->EdgeValue, reverseSense,
            MakeProjectedPcurve(
                model, *face.Surface, it->first->Span->Points,
                reverseSense == Orientation::Reversed),
            name + "_complement_coedge"));
    }
    Model::LinkLoop(patchOuter, patchUses);
    Model::LinkLoop(complementInner, complementUses);
    for (CoEdge* patchUse : patchUses)
    {
        for (CoEdge* complementUse : complementUses)
        {
            if (patchUse->Edge == complementUse->Edge)
            {
                Model::PairPartners(patchUse, complementUse);
                break;
            }
        }
    }
    return true;
}

[[nodiscard]] Shell* FindFaceShell(Body& body, const Face& face)
{
  for (Shell* shell : body.Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    if (std::find(shell->Faces.begin(), shell->Faces.end(), &face) !=
        shell->Faces.end())
    {
      return shell;
    }
  }
  return nullptr;
}

[[nodiscard]] bool IsNearlyFullCircleSpan(const AnalyticArcSpan& arc)
{
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    return arc.Circle != nullptr &&
           std::abs((arc.T1 - arc.T0) - kTwoPi) < 1e-5;
}

[[nodiscard]] bool ImprintClosedCircleOnPlanarFaceImpl(
    Model& model, Shell& shell, Face& face, const Point3d& center,
    const Vector3d& normal, double radius, double eps)
{
    if (face.Surface == nullptr ||
        face.Surface->Kind() != SurfaceKind::Plane || !(radius > eps) ||
        normal.norm() <= eps)
    {
        return false;
    }
    if (!PointOnOrInPlanarFace(face, center, eps))
    {
        return false;
    }

    const Vector3d unitNormal = normal.normalized();
    const std::string name = face.Name + "_circle_imprint";
    CircleCurve* curve =
        model.MakeCircle(center, unitNormal, radius, name + "_curve");
    Vertex* first = model.MakeVertex(
        model.MakePoint(curve->Eval(0.0), name + "_point0"), eps,
        name + "_vertex0");
    Vertex* second = model.MakeVertex(
        model.MakePoint(curve->Eval(std::numbers::pi), name + "_point1"), eps,
        name + "_vertex1");
    Edge* firstHalf =
        model.MakeEdge(curve, first, second, 0.0, std::numbers::pi,
                       face.Tolerance, name + "_edge0");
    Edge* secondHalf =
        model.MakeEdge(curve, second, first, std::numbers::pi,
                       2.0 * std::numbers::pi, face.Tolerance, name + "_edge1");

    const std::vector<Point3d> half0{
        curve->Eval(0.0), curve->Eval(std::numbers::pi)};
    const std::vector<Point3d> half1{
        curve->Eval(std::numbers::pi), curve->Eval(2.0 * std::numbers::pi)};

    Face* disk = model.MakeFace(face.Surface, face.Sense, name + "_disk");
    shell.Faces.push_back(disk);
    Loop* diskOuter = model.MakeLoop(disk, LoopType::Outer, name + "_disk_outer");
    CoEdge* diskFirst = model.MakeCoedge(
        firstHalf, Orientation::Forward,
        MakeProjectedPcurve(model, *face.Surface, half0, false),
        name + "_disk_coedge0");
    CoEdge* diskSecond = model.MakeCoedge(
        secondHalf, Orientation::Forward,
        MakeProjectedPcurve(model, *face.Surface, half1, false),
        name + "_disk_coedge1");
    Model::LinkLoop(diskOuter, std::array<CoEdge*, 2>{diskFirst, diskSecond});

    Loop* ringInner = model.MakeLoop(&face, LoopType::Inner, name + "_ring_inner");
    CoEdge* ringSecond = model.MakeCoedge(
        secondHalf, Orientation::Reversed,
        MakeProjectedPcurve(model, *face.Surface, half1, true),
        name + "_ring_coedge1");
    CoEdge* ringFirst = model.MakeCoedge(
        firstHalf, Orientation::Reversed,
        MakeProjectedPcurve(model, *face.Surface, half0, true),
        name + "_ring_coedge0");
    Model::LinkLoop(ringInner, std::array<CoEdge*, 2>{ringSecond, ringFirst});
    Model::PairPartners(diskFirst, ringFirst);
    Model::PairPartners(diskSecond, ringSecond);
    return true;
}

}  // namespace

bool ImprintClosedCircleOnSphereFace(Model& model, Shell& shell, Face& face,
                                     const Point3d& center,
                                     const Vector3d& normal, double radius,
                                     double eps)
{
    if (face.Surface == nullptr ||
        face.Surface->Kind() != SurfaceKind::Sphere || !(radius > eps) ||
        normal.norm() <= eps)
    {
        return false;
    }
    auto& sphere = static_cast<SphereSurface&>(*face.Surface);
    const Vector3d unitNormal = normal.normalized();
    const Vector3d centerOffset = center - sphere.Center();
    if (centerOffset.cross(unitNormal).norm() > eps ||
        std::abs(centerOffset.squaredNorm() + radius * radius -
                 sphere.Radius() * sphere.Radius()) >
            eps * std::max(1.0, sphere.Radius() * sphere.Radius()))
    {
        return false;
    }

    for (double latitude : {-0.5 * std::numbers::pi,
                            0.5 * std::numbers::pi})
    {
        const Point3d pole = sphere.Eval(0.0, latitude);
        const double planeDistance =
            std::abs(unitNormal.dot(pole - center));
        const double radialDistance =
            ((pole - center) -
             unitNormal * unitNormal.dot(pole - center))
                .norm();
        if (planeDistance <= eps &&
            std::abs(radialDistance - radius) <= eps)
        {
            return false;
        }
    }

    const std::string name = face.Name + "_circle_imprint";
    CircleCurve* curve =
        model.MakeCircle(center, unitNormal, radius, name + "_curve");
    Vertex* first = model.MakeVertex(
        model.MakePoint(curve->Eval(0.0), name + "_point0"), eps,
        name + "_vertex0");
    Vertex* second = model.MakeVertex(
        model.MakePoint(curve->Eval(std::numbers::pi), name + "_point1"), eps,
        name + "_vertex1");
    Edge* firstHalf =
        model.MakeEdge(curve, first, second, 0.0, std::numbers::pi, eps,
                       name + "_edge0");
    Edge* secondHalf =
        model.MakeEdge(curve, second, first, std::numbers::pi,
                       2.0 * std::numbers::pi, eps, name + "_edge1");

    Face* cap = model.MakeFace(face.Surface, face.Sense, name + "_cap");
    shell.Faces.push_back(cap);
    Loop* capOuter =
        model.MakeLoop(cap, LoopType::Outer, name + "_cap_outer");
    CoEdge* capFirst = model.MakeCoedge(
        firstHalf, Orientation::Forward, nullptr, name + "_cap_coedge0");
    CoEdge* capSecond = model.MakeCoedge(
        secondHalf, Orientation::Forward, nullptr, name + "_cap_coedge1");
    Model::LinkLoop(capOuter, std::array<CoEdge*, 2>{capFirst, capSecond});

    Loop* complementInner =
        model.MakeLoop(&face, LoopType::Inner, name + "_complement_inner");
    CoEdge* complementSecond = model.MakeCoedge(
        secondHalf, Orientation::Reversed, nullptr,
        name + "_complement_coedge1");
    CoEdge* complementFirst = model.MakeCoedge(
        firstHalf, Orientation::Reversed, nullptr,
        name + "_complement_coedge0");
    Model::LinkLoop(
        complementInner,
        std::array<CoEdge*, 2>{complementSecond, complementFirst});
    Model::PairPartners(capFirst, complementFirst);
    Model::PairPartners(capSecond, complementSecond);
    return true;
}

bool ImprintClosedCircleOnPlanarFace(Model& model, Shell& shell, Face& face,
                                     const Point3d& center,
                                     const Vector3d& normal, double radius,
                                     double eps)
{
    return ImprintClosedCircleOnPlanarFaceImpl(model, shell, face, center,
                                               normal, radius, eps);
}

SplitEdgeResult SplitEdgeAt(Model& model, Edge& edge, double t, double eps)
{
  SplitEdgeResult result;
  if (!edge.Curve || edge.Curve->Kind() != CurveKind::Line || !edge.V0 || !edge.V1)
  {
    return result;
  }
  if (t <= edge.T0 + eps || t >= edge.T1 - eps)
  {
    return result;
  }

  const auto& line = static_cast<const LineCurve&>(*edge.Curve);
  const Point3d splitPt = line.Eval(t);
  const double snapEps = SnapTolerance(eps);
  if (edge.V0 && PointsEqual(edge.V0->Position(), splitPt, snapEps))
  {
    result.SplitVertex = edge.V0;
    return result;
  }
  if (edge.V1 && PointsEqual(edge.V1->Position(), splitPt, snapEps))
  {
    result.SplitVertex = edge.V1;
    return result;
  }

  Vertex* vNew = model.MakeVertex(model.MakePoint(splitPt), edge.Tolerance,
                                  edge.Name + "_split");

  Vertex* const originalEnd = edge.V1;
  LineCurve* lineSecond = model.MakeLine(splitPt, originalEnd->Position());
  Edge* secondHalf =
      model.MakeEdge(lineSecond, vNew, originalEnd, 0.0, lineSecond->Length(),
                     edge.Tolerance, edge.Name + "_b");

  LineCurve* lineFirst = model.MakeLine(edge.V0->Position(), splitPt);
  edge.Curve = lineFirst;
  edge.V1 = vNew;
  edge.T1 = lineFirst->Length();

  std::vector<CoEdge*> radial = edge.Radial;
  std::vector<CoEdge*> splitCoedges;
  splitCoedges.reserve(radial.size());

  for (CoEdge* coedge : radial)
  {
    if (!coedge)
    {
      continue;
    }
    Curve2d* pcurveSecond = nullptr;
    if (coedge->Loop && coedge->Loop->Face && coedge->Loop->Face->Surface &&
        coedge->Loop->Face->Surface->Kind() == SurfaceKind::Plane)
    {
      const auto& plane =
          static_cast<const PlaneSurface&>(*coedge->Loop->Face->Surface);
      pcurveSecond = model.MakeLine2d(plane.ParamOf(splitPt),
                                    plane.ParamOf(originalEnd->Position()));
    }
    CoEdge* coedgeSecond =
        model.MakeCoedge(secondHalf, coedge->Sense, pcurveSecond,
                         coedge->Name + "_split");
    if (!coedge->Next || !coedge->Next->Prev || !coedge->Prev)
    {
      continue;
    }
    coedgeSecond->Loop = coedge->Loop;
    if (coedge->To() == coedgeSecond->From())
    {
      coedgeSecond->Next = coedge->Next;
      coedgeSecond->Prev = coedge;
      coedge->Next->Prev = coedgeSecond;
      coedge->Next = coedgeSecond;
    }
    else if (coedge->From() == coedgeSecond->To())
    {
      coedgeSecond->Next = coedge;
      coedgeSecond->Prev = coedge->Prev;
      coedge->Prev->Next = coedgeSecond;
      coedge->Prev = coedgeSecond;
    }
    else
    {
      continue;
    }
    splitCoedges.push_back(coedgeSecond);
  }

  if (splitCoedges.size() == 2)
  {
    Model::PairPartners(splitCoedges[0], splitCoedges[1]);
  }

  result.Ok = true;
  result.FirstHalf = &edge;
  result.SecondHalf = secondHalf;
  result.SplitVertex = vNew;
  return result;
}

bool ImprintSegmentOnPlanarFace(Model& model, Shell& shell, Face& face,
                                const Point3d& start, const Point3d& end, double eps)
{
  if (!face.Surface || face.Surface->Kind() != SurfaceKind::Plane)
  {
    return false;
  }
  Face* targetFace = &face;
  if (Face* resolved = ResolveImprintFace(shell, *face.Surface, start, end, eps))
  {
    targetFace = resolved;
  }
  if (!targetFace->Surface || targetFace->Surface->Kind() != SurfaceKind::Plane)
  {
    return false;
  }
  auto& plane = static_cast<PlaneSurface&>(*targetFace->Surface);
  Loop* outer = targetFace->OuterLoop();
  if (!outer || !outer->First)
  {
    return false;
  }

  const auto quantize = [&](const Point3d& point) -> Point3d
  {
    const double inv = 1.0 / eps;
    return Point3d{std::llround(point.x() * inv) / inv,
                   std::llround(point.y() * inv) / inv,
                   std::llround(point.z() * inv) / inv};
  };
  const auto projectToFacePlane = [&](const Point3d& point) -> Point3d
  {
    const Vector3d normal = plane.Normal(0.0, 0.0).normalized();
    const double dist = normal.dot(point - plane.Origin());
    return point - normal * dist;
  };
  const auto snapNearBoundary = [&](const Point3d& point) -> Point3d
  {
    const Point3d onPlane = projectToFacePlane(point);
    const BoundarySnapResult snap = SnapToLoopBoundary(*outer, onPlane, eps);
    return snap.Found ? snap.Point : onPlane;
  };
  const Point3d useStart = snapNearBoundary(quantize(start));
  const Point3d useEnd = snapNearBoundary(quantize(end));

  const Vector3d seg = useEnd - useStart;
  const double segLen = seg.norm();
  if (segLen <= eps)
  {
    return false;
  }
  const Vector3d segUnit = seg / segLen;
  const Vector3d normal = plane.Normal(0.0, 0.0).normalized();
  if (std::abs(normal.dot(segUnit)) > eps ||
      std::abs(normal.dot(useStart - plane.Origin())) > eps)
  {
    return false;
  }

  const auto faceClip = ClipLineToPlanarFace(*targetFace, useStart, segUnit, eps);
  if (!faceClip)
  {
    return false;
  }
  if (faceClip->second < -eps || faceClip->first > segLen + eps)
  {
    return false;
  }

  Point3d clipStart = useStart;
  Point3d clipEnd = useStart + segUnit * std::min(segLen, faceClip->second);
  if (faceClip->first < -eps)
  {
    clipStart = useStart + segUnit * faceClip->first;
  }
  else if (faceClip->first > eps)
  {
    clipStart = useStart + segUnit * faceClip->first;
  }
  if (faceClip->second > segLen + eps)
  {
    clipEnd = useStart + segUnit * faceClip->second;
  }
  clipStart = snapNearBoundary(clipStart);
  clipEnd = snapNearBoundary(clipEnd);
  const double imprintLen = (clipEnd - clipStart).norm();
  if (imprintLen <= eps)
  {
    return false;
  }

  const auto vertexAtBoundaryPoint = [&](const Point3d& point) -> Vertex*
  {
    CoEdge* coedge = ResolveCoedgeAtPoint(*outer, point, eps);
    if (coedge == nullptr)
    {
      return nullptr;
    }
    if (PointsEqual(coedge->From()->Position(), point, eps))
    {
      return coedge->From();
    }
    if (PointsEqual(coedge->To()->Position(), point, eps))
    {
      return coedge->To();
    }
    return nullptr;
  };
  if (Vertex* vStartBound = vertexAtBoundaryPoint(clipStart))
  {
    if (Vertex* vEndBound = vertexAtBoundaryPoint(clipEnd))
    {
      if (vStartBound != vEndBound)
      {
        const std::vector<CoEdge*> alongBoundary =
            CollectLoopPath(*outer, *vStartBound, *vEndBound);
        if (alongBoundary.size() == 1U)
        {
          return true;
        }
      }
    }
  }

  struct SegmentHit
  {
    double SegT{0.0};
    Point3d Point;
    CoEdge* Coedge{nullptr};
  };
  std::vector<SegmentHit> hits;

  auto addHit = [&](double segT, const Point3d& point, CoEdge* coedge)
  {
    for (const SegmentHit& existing : hits)
    {
      if (PointsEqual(existing.Point, point, eps))
      {
        return;
      }
    }
    hits.push_back(SegmentHit{segT, point, coedge});
  };

  addHit(0.0, clipStart, nullptr);
  addHit(imprintLen, clipEnd, nullptr);

  CoEdge* walk = outer->First;
  std::size_t walkGuard = 0;
  do
  {
    if (++walkGuard > 64)
    {
      return false;
    }
    if (!walk->Edge || !walk->Edge->Curve ||
        walk->Edge->Curve->Kind() != CurveKind::Line)
    {
      walk = walk->Next;
      continue;
    }
    const Point3d edgeStart = walk->From()->Position();
    const Point3d edgeEnd = walk->To()->Position();
    const auto intersection =
        SegmentSegmentIntersect(edgeStart, edgeEnd, clipStart, clipEnd, eps);
    if (!intersection)
    {
      walk = walk->Next;
      continue;
    }
    const double segT = (*intersection - clipStart).dot(segUnit);
    if (segT >= -eps && segT <= imprintLen + eps)
    {
      addHit(std::clamp(segT, 0.0, imprintLen), *intersection, walk);
    }
    walk = walk->Next;
  } while (walk && walk != outer->First);

  std::sort(hits.begin(), hits.end(),
            [](const SegmentHit& a, const SegmentHit& b)
            {
              return a.SegT < b.SegT;
            });

  struct ResolvedHit
  {
    CoEdge* Coedge{nullptr};
    Point3d Point;
    bool NeedsSplit{false};
    double EdgeParam{0.0};
  };
  std::vector<ResolvedHit> resolved;
  resolved.reserve(hits.size());
  for (const SegmentHit& hit : hits)
  {
    CoEdge* coedge = hit.Coedge != nullptr
                         ? hit.Coedge
                         : ResolveCoedgeAtPoint(*outer, hit.Point, eps);
    if (coedge == nullptr || coedge->Edge == nullptr)
    {
      return false;
    }
    ResolvedHit entry;
    entry.Coedge = coedge;
    entry.Point = hit.Point;
    if (PointsEqual(coedge->From()->Position(), hit.Point, eps))
    {
      resolved.push_back(entry);
      continue;
    }
    if (PointsEqual(coedge->To()->Position(), hit.Point, eps))
    {
      resolved.push_back(entry);
      continue;
    }
    entry.NeedsSplit = true;
    entry.EdgeParam = EdgeParamAtPoint(*coedge->Edge, *coedge, hit.Point, eps);
    resolved.push_back(entry);
  }

  std::vector<Vertex*> chain;
  chain.reserve(resolved.size());
  const auto vertexForHit = [&](const ResolvedHit& hit) -> Vertex*
  {
    if (hit.NeedsSplit)
    {
      return nullptr;
    }
    if (PointsEqual(hit.Coedge->From()->Position(), hit.Point, eps))
    {
      return hit.Coedge->From();
    }
    return hit.Coedge->To();
  };
  if (Vertex* pvStart = vertexForHit(resolved.front()))
  {
    if (Vertex* pvEnd = vertexForHit(resolved.back()))
    {
      if (pvStart != pvEnd)
      {
        const std::size_t fwd = CollectLoopPath(*outer, *pvStart, *pvEnd).size();
        const std::size_t bwd = CollectLoopPath(*outer, *pvEnd, *pvStart).size();
        if (fwd <= 1U || bwd <= 1U)
        {
          return true;
        }
        if (fwd == 0U || bwd == 0U)
        {
          return false;
        }
      }
    }
  }

  bool splitAny = false;
  for (const ResolvedHit& hit : resolved)
  {
    if (!hit.NeedsSplit)
    {
      if (PointsEqual(hit.Coedge->From()->Position(), hit.Point, eps))
      {
        chain.push_back(hit.Coedge->From());
      }
      else
      {
        chain.push_back(hit.Coedge->To());
      }
      continue;
    }
    const SplitEdgeResult split =
        SplitEdgeAt(model, *hit.Coedge->Edge, hit.EdgeParam, eps);
    if (!split.Ok || split.SplitVertex == nullptr)
    {
      return false;
    }
    splitAny = true;
    chain.push_back(split.SplitVertex);
  }

  if (chain.size() < 2)
  {
    return splitAny;
  }

  Vertex* vStart = chain.front();
  Vertex* vEnd = chain.back();
  if (vStart == vEnd)
  {
    return splitAny;
  }

  if (HasImprintChord(*outer, *vStart, *vEnd, eps))
  {
    return true;
  }

  const std::vector<CoEdge*> forwardPath =
      CollectLoopPath(*outer, *vStart, *vEnd);
  const std::vector<CoEdge*> backwardPath =
      CollectLoopPath(*outer, *vEnd, *vStart);
  if (forwardPath.empty() || backwardPath.empty())
  {
    return false;
  }
  // A chord along an existing boundary (one edge, or several colinear pieces
  // of one edge) must not spawn a zero-area sliver face.
  if (forwardPath.size() <= 1U || backwardPath.size() <= 1U ||
      PathLiesOnSegment(forwardPath, *vStart, *vEnd, eps) ||
      PathLiesOnSegment(backwardPath, *vStart, *vEnd, eps))
  {
    return true;
  }

  if (SplitFaceAtChord(model, shell, *targetFace, *outer, *vStart, *vEnd))
  {
    return true;
  }
  return splitAny;
}

ImprintResult RunTopologicalImprint(PipelineState& state)
{
  ImprintResult result;
  Body* bodyA = state.WorkingBodyA != nullptr ? state.WorkingBodyA
                                               : const_cast<Body*>(state.BodyA);
  Body* bodyB = state.WorkingBodyB != nullptr ? state.WorkingBodyB
                                               : const_cast<Body*>(state.BodyB);
  if (bodyA == nullptr || bodyB == nullptr || state.TargetModel == nullptr)
  {
    result.Diagnostics = "RunTopologicalImprint: missing operands or model";
    return result;
  }
  if (state.IntersectionGraph.Segments.empty() &&
      state.IntersectionGraph.Circles.empty())
  {
    result.Ok = true;
    return result;
  }

  const double eps = std::max(state.Ctx.fuzzy, 1e-9);
  const double invEps = 1.0 / SnapTolerance(eps);
  std::set<PlanarImprintKey> imprintedOnA;
  std::set<PlanarImprintKey> imprintedOnB;
  for (const IntersectionSegment& segment : state.IntersectionGraph.Segments)
  {
    if (segment.CurveKind == CurveKind::Circle)
    {
      continue;
    }
    const Point3d start{segment.Start.x(), segment.Start.y(), segment.Start.z()};
    const Point3d end{segment.End.x(), segment.End.y(), segment.End.z()};
    if (segment.FaceA != nullptr && segment.FaceA->Surface != nullptr &&
        segment.FaceA->Surface->Kind() == SurfaceKind::Plane)
    {
      const PlanarImprintKey keyA =
          MakePlanarImprintKey(*segment.FaceA->Surface, start, end, invEps);
      if (imprintedOnA.insert(keyA).second)
      {
        Shell* shellA = FindFaceShell(*bodyA, *segment.FaceA);
        if (shellA != nullptr)
        {
          Face* faceA = ResolveImprintFace(*shellA, *segment.FaceA->Surface, start, end, eps);
          if (faceA == nullptr)
          {
            faceA = segment.FaceA;
          }
          (void)ImprintSegmentOnPlanarFace(*state.TargetModel, *shellA, *faceA, start, end,
                                           eps);
        }
      }
    }

    if (segment.FaceB != nullptr && segment.FaceB->Surface != nullptr &&
        segment.FaceB->Surface->Kind() == SurfaceKind::Plane)
    {
      const PlanarImprintKey keyB =
          MakePlanarImprintKey(*segment.FaceB->Surface, start, end, invEps);
      if (imprintedOnB.insert(keyB).second)
      {
        Shell* shellB = FindFaceShell(*bodyB, *segment.FaceB);
        if (shellB != nullptr)
        {
          Face* faceB = ResolveImprintFace(*shellB, *segment.FaceB->Surface, start, end, eps);
          if (faceB == nullptr)
          {
            faceB = segment.FaceB;
          }
          (void)ImprintSegmentOnPlanarFace(*state.TargetModel, *shellB, *faceB, start, end,
                                           eps);
        }
      }
    }
  }

  std::unordered_map<Face*, std::vector<AnalyticArcSpan>> sphereArcs;
  const auto findOwningShell = [&](Face* face) -> Shell*
  {
    if (face == nullptr)
    {
      return nullptr;
    }
    if (Shell* shell = FindFaceShell(*bodyA, *face))
    {
      return shell;
    }
    return FindFaceShell(*bodyB, *face);
  };
  for (const IntersectionCircle& circle : state.IntersectionGraph.Circles)
  {
    if (circle.Closed || circle.FaceA == nullptr || circle.FaceB == nullptr)
    {
      continue;
    }
    std::vector<AnalyticArcSpan> spans =
        CollectAnalyticArcSpans(state.IntersectionGraph, circle, eps);
    if (spans.empty())
    {
      continue;
    }

    Shell* planeShell = findOwningShell(circle.FaceA);
    if (planeShell == nullptr || circle.FaceA->Surface == nullptr)
    {
      result.Diagnostics =
          "RunTopologicalImprint: plane-circle face has no shell";
      return result;
    }
    Shell* sphereShell = findOwningShell(circle.FaceB);
    if (sphereShell == nullptr || circle.FaceB == nullptr ||
        circle.FaceB->Surface == nullptr ||
        circle.FaceB->Surface->Kind() != SurfaceKind::Sphere)
    {
      result.Diagnostics =
          "RunTopologicalImprint: sphere-circle face has no shell";
      return result;
    }

    for (const AnalyticArcSpan& arc : spans)
    {
      if (IsNearlyFullCircleSpan(arc))
      {
        Face* planeFace = ResolveImprintFace(
            *planeShell, *circle.FaceA->Surface, circle.Center, circle.Center,
            eps);
        if (planeFace == nullptr)
        {
          planeFace = circle.FaceA;
        }
        Face* sphereFace = circle.FaceB;
        if (Face* resolved = ResolveImprintFace(
                *sphereShell, *circle.FaceB->Surface, circle.Center,
                circle.Center, eps))
        {
          sphereFace = resolved;
        }
        if (!ImprintClosedCircleOnPlanarFaceImpl(
                *state.TargetModel, *planeShell, *planeFace, circle.Center,
                circle.Normal, circle.Radius, eps) ||
            !ImprintClosedCircleOnSphereFace(
                *state.TargetModel, *sphereShell, *sphereFace, circle.Center,
                circle.Normal, circle.Radius, eps))
        {
          result.Diagnostics =
              "RunTopologicalImprint: failed closed plane-sphere circle";
          return result;
        }
        continue;
      }

      Face* planeFace = ResolveImprintFace(
          *planeShell, *circle.FaceA->Surface, arc.Points.front(),
          arc.Points.back(), eps);
      if (planeFace == nullptr)
      {
        planeFace = circle.FaceA;
      }
      if (!SplitPlanarFaceAtArc(*state.TargetModel, *planeShell, *planeFace,
                                arc, eps, state.WorkingBodyA,
                                state.WorkingBodyB, state.Op))
      {
        if (HasCircleImprintArcOnFace(*planeFace, arc, eps))
        {
          sphereArcs[circle.FaceB].push_back(arc);
          continue;
        }
        result.Diagnostics =
            "RunTopologicalImprint: failed planar circle arc";
        return result;
      }
      sphereArcs[circle.FaceB].push_back(arc);
    }
  }
  for (auto& [sphereFace, arcs] : sphereArcs)
  {
    if (sphereFace == nullptr || arcs.empty())
    {
      continue;
    }
    Shell* sphereShell = findOwningShell(sphereFace);
    if (sphereShell == nullptr)
    {
      continue;
    }
    const auto components = PartitionArcComponents(
             arcs, sphereFace->Surface != nullptr &&
                        sphereFace->Surface->Kind() == SurfaceKind::Sphere
                        ? static_cast<const SphereSurface*>(sphereFace->Surface)
                        : nullptr,
             eps);
    for (const std::vector<AnalyticArcSpan>& component : components)
    {
      if (component.size() < 2U)
      {
        continue;
      }
      Face* resolvedFace = sphereFace;
      if (Face* resolved = ResolveImprintFace(
              *sphereShell, *sphereFace->Surface, component.front().Points.front(),
              component.back().Points.back(), eps))
      {
        resolvedFace = resolved;
      }
      if (!ImprintArcCycleOnSphereFace(*state.TargetModel, *sphereShell,
                                       *resolvedFace, component, eps))
      {
        if (component.size() != arcs.size())
        {
          continue;
        }
        result.Diagnostics =
            "RunTopologicalImprint: failed spherical arc cycle";
        return result;
      }
    }
  }

  for (const IntersectionCircle& circle : state.IntersectionGraph.Circles)
  {
    if (!circle.Closed)
    {
      continue;
    }
    const auto imprintSphere = [&](Body& body, Face* face) -> bool
    {
      if (face == nullptr || face->Surface == nullptr ||
          face->Surface->Kind() != SurfaceKind::Sphere)
      {
        return true;
      }
      Shell* shell = FindFaceShell(body, *face);
      return shell != nullptr &&
             ImprintClosedCircleOnSphereFace(
                 *state.TargetModel, *shell, *face, circle.Center,
                 circle.Normal, circle.Radius, eps);
    };
    if (!imprintSphere(*bodyA, circle.FaceA) ||
        !imprintSphere(*bodyB, circle.FaceB))
    {
      result.Diagnostics =
          "RunTopologicalImprint: unsupported sphere circle seam or pole";
      return result;
    }
  }

  state.IntersectionGraph.Fragments.clear();
  CollectBodyFragments(*bodyA, state.IntersectionGraph);
  CollectBodyFragments(*bodyB, state.IntersectionGraph);

  const double weldEps = 1e-5;
  for (Shell* shell : bodyA->Shells)
  {
    if (shell != nullptr)
    {
      WeldShellVertices(*shell, weldEps);
    }
  }
  for (Shell* shell : bodyB->Shells)
  {
    if (shell != nullptr)
    {
      WeldShellVertices(*shell, weldEps);
    }
  }

  result.Ok = true;
  return result;
}

ImprintResult RunImprint(PipelineState& state)
{
  ImprintResult result;
  if (state.BodyA == nullptr || state.BodyB == nullptr || state.TargetModel == nullptr)
  {
    result.Diagnostics = "RunImprint: missing operands or model";
    return result;
  }

  return RunTopologicalImprint(state);
}

}  // namespace brep::boolean