#include "brep/bool/SolidClassifier.h"

#include "brep/Geometry.h"
#include "brep/internal/Polygon2d.h"
#include "brep/Math.h"
#include "brep/Topology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

namespace brep::boolean
{
namespace
{

constexpr int kRayParitySamples = 5;

using brep::internal::PointInPolygon2d;

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

bool RayPlaneHit(const PlaneSurface& plane, const Point3d& origin, const Vector3d& dir,
                 double eps, double& outT)
{
  const Vector3d normal = plane.Normal(0.0, 0.0);
  const double denom = normal.dot(dir);
  if (std::abs(denom) < eps)
  {
    return false;
  }
  const double t = normal.dot(plane.Origin() - origin) / denom;
  if (t <= eps)
  {
    return false;
  }
  outT = t;
  return true;
}

[[nodiscard]] bool PointInSphereFaceTrim(const Face& face,
                                         const Point3d& point, double eps)
{
    const auto& sphere = static_cast<const SphereSurface&>(*face.Surface);
    for (const Loop* loop : face.Loops)
    {
        if (loop == nullptr || loop->First == nullptr ||
            loop->First->Edge == nullptr ||
            loop->First->Edge->Curve == nullptr ||
            loop->First->Edge->Curve->Kind() != CurveKind::Circle)
        {
            continue;
        }
        if (loop->First->Edge->Name.find("_sphere_imprint_edge") !=
            std::string::npos)
        {
            Vector3d direction;
            std::unordered_set<const Vertex*> seen;
            loop->ForEachCoedge([&](const CoEdge& coedge)
            {
                const Vertex* vertex = coedge.From();
                if (vertex != nullptr && seen.insert(vertex).second)
                {
                    direction += vertex->Position() - sphere.Center();
                }
            });
            if (direction.norm() <= eps)
            {
                return false;
            }
            const Point3d reference =
                sphere.Center() +
                direction.normalized() * sphere.Radius();
            bool inside = true;
            loop->ForEachCoedge([&](const CoEdge& coedge)
            {
                if (!inside || coedge.Edge == nullptr ||
                    coedge.Edge->Curve == nullptr ||
                    coedge.Edge->Curve->Kind() != CurveKind::Circle)
                {
                    inside = false;
                    return;
                }
                const auto& boundary =
                    static_cast<const CircleCurve&>(*coedge.Edge->Curve);
                const double referenceSide = boundary.Normal().dot(
                    reference - boundary.Center());
                const double pointSide =
                    boundary.Normal().dot(point - boundary.Center());
                if ((referenceSide >= 0.0 && pointSide < -eps) ||
                    (referenceSide < 0.0 && pointSide > eps))
                {
                    inside = false;
                }
            });
            return loop->Type == LoopType::Inner ? !inside : inside;
        }
        const auto& circle =
            static_cast<const CircleCurve&>(*loop->First->Edge->Curve);
        if ((circle.Center() - sphere.Center()).norm() <= eps)
        {
            continue;
        }
        const double signedDistance =
            circle.Normal().dot(point - circle.Center());
        return loop->Type == LoopType::Inner ? signedDistance <= eps
                                             : signedDistance >= -eps;
    }
    return true;
}

bool RaySphereHit(const Face& face, const SphereSurface& sphere,
                  const Point3d& origin, const Vector3d& dir, double eps,
                  double& outT)
{
    const Vector3d oc = origin - sphere.Center();
    const double a = dir.dot(dir);
    const double b = 2.0 * oc.dot(dir);
    const double c = oc.dot(oc) - sphere.Radius() * sphere.Radius();
    const double disc = b * b - 4.0 * a * c;
    if (disc < 0.0)
    {
        return false;
    }
    const double sqrtDisc = std::sqrt(disc);
    const double inv2a = 1.0 / (2.0 * a);
    const std::array<double, 2> roots{
        (-b - sqrtDisc) * inv2a,
        (-b + sqrtDisc) * inv2a,
    };
    for (double t : roots)
    {
        if (t > eps &&
            PointInSphereFaceTrim(face, origin + dir * t, eps))
        {
            outT = t;
            return true;
        }
    }
    return false;
}

bool RayCylinderHit(const CylinderSurface& cylinder, const Point3d& origin,
                    const Vector3d& dir, double eps, double& outT)
{
  const Vector3d axis = cylinder.Axis().normalized();
  const Vector3d oc = origin - cylinder.Origin();
  const Vector3d dirPerp = dir - axis * dir.dot(axis);
  const Vector3d ocPerp = oc - axis * oc.dot(axis);
  const double a = dirPerp.dot(dirPerp);
  if (a <= eps)
  {
    return false;
  }
  const double b = 2.0 * ocPerp.dot(dirPerp);
  const double c = ocPerp.dot(ocPerp) - cylinder.Radius() * cylinder.Radius();
  const double disc = b * b - 4.0 * a * c;
  if (disc < 0.0)
  {
    return false;
  }
  const double sqrtDisc = std::sqrt(disc);
  const double inv2a = 1.0 / (2.0 * a);
  const double t0 = (-b - sqrtDisc) * inv2a;
  const double t1 = (-b + sqrtDisc) * inv2a;
  const double t = (t0 > eps) ? t0 : ((t1 > eps) ? t1 : std::numeric_limits<double>::max());
  if (t == std::numeric_limits<double>::max())
  {
    return false;
  }
  outT = t;
  return true;
}

bool RayHitsBoundedFace(const Face& face, const Point3d& origin, const Vector3d& dir,
                        double eps, double& outT)
{
  if (face.Surface == nullptr)
  {
    return false;
  }
  if (face.Surface->Kind() == SurfaceKind::Plane)
  {
    const auto& plane = static_cast<const PlaneSurface&>(*face.Surface);
    if (!RayPlaneHit(plane, origin, dir, eps, outT))
    {
      return false;
    }
    const Point3d hit = origin + dir * outT;
    const std::vector<Point2d> polygon = CollectFaceLoopUv(face);
    if (polygon.empty())
    {
      return false;
    }
    return PointInPolygon2d(plane.ParamOf(hit), polygon, eps);
  }
  switch (face.Surface->Kind())
  {
    case SurfaceKind::Sphere:
      return RaySphereHit(face,
                          static_cast<const SphereSurface&>(*face.Surface),
                          origin, dir, eps, outT);
    case SurfaceKind::Cylinder:
      return RayCylinderHit(static_cast<const CylinderSurface&>(*face.Surface), origin,
                            dir, eps, outT);
    default:
      return false;
  }
}

[[nodiscard]] bool PointOnBoundedFace(const Face& face, const Point3d& point,
                                      double eps)
{
  if (face.Surface == nullptr)
  {
    return false;
  }
  switch (face.Surface->Kind())
  {
    case SurfaceKind::Plane:
    {
      const auto& plane = static_cast<const PlaneSurface&>(*face.Surface);
      if (std::abs(plane.Normal(0.0, 0.0).dot(point - plane.Origin())) > eps)
      {
        return false;
      }
      return PointInPolygon2d(plane.ParamOf(point), CollectFaceLoopUv(face),
                              eps);
    }
    case SurfaceKind::Sphere:
    {
      const auto& sphere = static_cast<const SphereSurface&>(*face.Surface);
      return std::abs((point - sphere.Center()).norm() - sphere.Radius()) <=
                 eps &&
             PointInSphereFaceTrim(face, point, eps);
    }
    case SurfaceKind::Cylinder:
    {
      const auto& cylinder =
          static_cast<const CylinderSurface&>(*face.Surface);
      const Vector3d offset = point - cylinder.Origin();
      const Vector3d radial =
          offset - cylinder.Axis() * offset.dot(cylinder.Axis());
      return std::abs(radial.norm() - cylinder.Radius()) <= eps;
    }
    default:
      return false;
  }
}

bool RayHitsFace(const Face& face, const Point3d& origin, const Vector3d& dir, double eps,
                 double& outT)
{
  return RayHitsBoundedFace(face, origin, dir, eps, outT);
}

int CountRayCrossings(const Body& solid, const Point3d& origin, const Vector3d& dir,
                      double eps)
{
  int crossings = 0;
  for (const Shell* shell : solid.Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (const Face* face : shell->Faces)
    {
      if (face == nullptr)
      {
        continue;
      }
      double t = 0.0;
      if (RayHitsBoundedFace(*face, origin, dir, eps, t))
      {
        ++crossings;
      }
    }
  }
  return crossings;
}

Vector3d PickProbeDirection(const Point3d& /*p*/, double eps)
{
  static const Vector3d kDirs[] = {
      {1.0, 0.13, 0.07},
      {0.11, 1.0, 0.05},
      {0.03, 0.17, 1.0},
  };
  for (const Vector3d& d : kDirs)
  {
    if (d.norm() > eps)
    {
      return d.normalized();
    }
  }
  return {1.0, 0.0, 0.0};
}

}  // namespace

SolidClass ClassifyPointInBody(const Body& solid, const Point3d& p, double eps)
{
  if (solid.Shells.empty())
  {
    return SolidClass::Out;
  }

  bool hasSphereSurface = false;
  bool outsideAllSphereBounds = true;
  bool insideSomeSphereBounds = false;
  for (const Shell* shell : solid.Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (const Face* face : shell->Faces)
    {
      if (face == nullptr || face->Surface == nullptr ||
          face->Surface->Kind() != SurfaceKind::Sphere)
      {
        continue;
      }
      hasSphereSurface = true;
      const auto& sphere =
          static_cast<const SphereSurface&>(*face->Surface);
      const double distance = (p - sphere.Center()).norm();
      if (distance <= sphere.Radius() + eps)
      {
        outsideAllSphereBounds = false;
      }
      if (distance < sphere.Radius() - eps)
      {
        insideSomeSphereBounds = true;
      }
    }
  }
  if (hasSphereSurface && outsideAllSphereBounds)
  {
    return SolidClass::Out;
  }
  if (hasSphereSurface && insideSomeSphereBounds)
  {
    return SolidClass::In;
  }

  for (const Shell* shell : solid.Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (const Face* face : shell->Faces)
    {
      if (face != nullptr && PointOnBoundedFace(*face, p, eps))
      {
        return SolidClass::On;
      }
    }
  }

  const Vector3d dir = PickProbeDirection(p, eps);
  int insideVotes = 0;
  int outsideVotes = 0;

  for (int i = 0; i < kRayParitySamples; ++i)
  {
    const Point3d origin = p + dir * (static_cast<double>(i + 1) * eps * 10.0);
    const int crossings = CountRayCrossings(solid, origin, dir, eps);
    if ((crossings % 2) == 1)
    {
      ++insideVotes;
    }
    else
    {
      ++outsideVotes;
    }
  }

  if (insideVotes > outsideVotes)
  {
    return SolidClass::In;
  }
  if (outsideVotes > insideVotes)
  {
    return SolidClass::Out;
  }
  return SolidClass::On;
}

}  // namespace brep::boolean
