#include "brep/bool/IntersectorRegistry.h"

#include "brep/bool/IntersectPlaneCylinder.h"
#include "brep/bool/IntersectPlanePlane.h"
#include "brep/bool/IntersectPlaneSphere.h"
#include "brep/bool/IntersectSphereCylinder.h"
#include "brep/bool/IntersectSphereSphere.h"

#include <string>

namespace brep::boolean
{
namespace
{

IntersectProbe FromPlanePlane(const Face& faceA, const Face& faceB,
                              const BooleanContext& ctx)
{
  const auto& planeA = static_cast<const PlaneSurface&>(*faceA.Surface);
  const auto& planeB = static_cast<const PlaneSurface&>(*faceB.Surface);
  const auto intersection = IntersectPlanePlane(planeA, planeB, ctx);
  if (intersection.status == PlanePlaneStatus::Line ||
      intersection.status == PlanePlaneStatus::Coincident)
  {
    return IntersectProbe::Nonempty;
  }
  return IntersectProbe::Empty;
}

IntersectProbe FromPlaneSphere(const Face& facePlane, const Face& faceSphere,
                               const BooleanContext& ctx)
{
  const auto& plane = static_cast<const PlaneSurface&>(*facePlane.Surface);
  const auto& sphere = static_cast<const SphereSurface&>(*faceSphere.Surface);
  const auto intersection = IntersectPlaneSphere(plane, sphere, ctx);
  return intersection.status == PlaneSphereStatus::Empty ? IntersectProbe::Empty
                                                         : IntersectProbe::Nonempty;
}

IntersectProbe FromSphereSphere(const Face& faceA, const Face& faceB,
                                const BooleanContext& ctx)
{
  const auto& sphereA = static_cast<const SphereSurface&>(*faceA.Surface);
  const auto& sphereB = static_cast<const SphereSurface&>(*faceB.Surface);
  const auto intersection = IntersectSphereSphere(sphereA, sphereB, ctx);
  if (intersection.status == SphereSphereStatus::Separate ||
      intersection.status == SphereSphereStatus::Contained)
  {
    return IntersectProbe::Empty;
  }
  return IntersectProbe::Nonempty;
}

IntersectProbe FromPlaneCylinder(const Face& facePlane, const Face& faceCylinder,
                                 const BooleanContext& ctx)
{
  const auto& plane = static_cast<const PlaneSurface&>(*facePlane.Surface);
  const auto& cylinder = static_cast<const CylinderSurface&>(*faceCylinder.Surface);
  const auto intersection = IntersectPlaneCylinder(plane, cylinder, ctx);
  return intersection.status == PlaneCylinderStatus::Empty ? IntersectProbe::Empty
                                                           : IntersectProbe::Nonempty;
}

IntersectProbe FromSphereCylinder(const Face& faceSphere, const Face& faceCylinder,
                                  const BooleanContext& ctx)
{
  const auto& sphere = static_cast<const SphereSurface&>(*faceSphere.Surface);
  const auto& cylinder = static_cast<const CylinderSurface&>(*faceCylinder.Surface);
  const auto intersection = IntersectSphereCylinder(sphere, cylinder, ctx);
  if (intersection.status == SphereCylinderStatus::Unsupported)
  {
    return IntersectProbe::Unsupported;
  }
  return intersection.status == SphereCylinderStatus::Empty ? IntersectProbe::Empty
                                                            : IntersectProbe::Nonempty;
}

IntersectProbe Dispatch(SurfaceKind kindA, SurfaceKind kindB, const Face& faceA,
                        const Face& faceB, const BooleanContext& ctx)
{
  if (kindA == SurfaceKind::Plane && kindB == SurfaceKind::Plane)
  {
    return FromPlanePlane(faceA, faceB, ctx);
  }
  if (kindA == SurfaceKind::Plane && kindB == SurfaceKind::Sphere)
  {
    return FromPlaneSphere(faceA, faceB, ctx);
  }
  if (kindA == SurfaceKind::Sphere && kindB == SurfaceKind::Plane)
  {
    return FromPlaneSphere(faceB, faceA, ctx);
  }
  if (kindA == SurfaceKind::Sphere && kindB == SurfaceKind::Sphere)
  {
    return FromSphereSphere(faceA, faceB, ctx);
  }
  if (kindA == SurfaceKind::Plane && kindB == SurfaceKind::Cylinder)
  {
    return FromPlaneCylinder(faceA, faceB, ctx);
  }
  if (kindA == SurfaceKind::Cylinder && kindB == SurfaceKind::Plane)
  {
    return FromPlaneCylinder(faceB, faceA, ctx);
  }
  if (kindA == SurfaceKind::Sphere && kindB == SurfaceKind::Cylinder)
  {
    return FromSphereCylinder(faceA, faceB, ctx);
  }
  if (kindA == SurfaceKind::Cylinder && kindB == SurfaceKind::Sphere)
  {
    return FromSphereCylinder(faceB, faceA, ctx);
  }
  return IntersectProbe::Unsupported;
}

}  // namespace

IntersectorRegistry::IntersectorRegistry() = default;

IntersectProbe IntersectorRegistry::Probe(const Face& faceA, const Face& faceB,
                                          const BooleanContext& ctx) const
{
  if (!faceA.Surface || !faceB.Surface)
  {
    return IntersectProbe::Unsupported;
  }
  const SurfaceKind kindA = faceA.Surface->Kind();
  const SurfaceKind kindB = faceB.Surface->Kind();
  return Dispatch(kindA, kindB, faceA, faceB, ctx);
}

BroadphaseProbe IntersectorRegistry::ProbeBodyPair(
    const Body& bodyA, const Body& bodyB, spatial::BuildQuality quality,
    const BooleanContext& ctx) const
{
  BroadphaseProbe probe;
  probe.Quality = quality;
  const auto candidates = CollectFacePairCandidates(bodyA, bodyB, quality);
  probe.CandidatePairs = candidates.size();
  for (const FacePairCandidate& candidate : candidates)
  {
    if (!candidate.a || !candidate.b)
    {
      ++probe.UnsupportedPairs;
      continue;
    }
    ++probe.AttemptedIntersects;
    switch (Probe(*candidate.a, *candidate.b, ctx))
    {
    case IntersectProbe::Nonempty:
      ++probe.NonemptyIntersects;
      break;
    case IntersectProbe::Empty:
      break;
    case IntersectProbe::Unsupported:
      ++probe.UnsupportedPairs;
      break;
    }
  }
  const char* qualityName =
      quality == spatial::BuildQuality::Sah ? "Sah" : "Median";
  probe.Summary = std::string("intersector_registry(") + qualityName + "): " +
                  std::to_string(probe.CandidatePairs) + " candidate face pairs, " +
                  std::to_string(probe.NonemptyIntersects) +
                  " nonempty analytic intersects, " +
                  std::to_string(probe.UnsupportedPairs) + " unsupported";
  return probe;
}

}  // namespace brep::boolean
