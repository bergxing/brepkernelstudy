#include "brep/bool/IntersectorRegistry.h"

#include "brep/bool/IntersectPlaneCylinder.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/bool/IntersectPlanePlane.h"
#include "brep/bool/IntersectPlaneSphere.h"
#include "brep/bool/IntersectSphereCylinder.h"
#include "brep/bool/IntersectSphereSphere.h"

#include <string>
#include <utility>

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

[[nodiscard]] std::vector<IntersectionSegment> IntersectFaces(
    SurfaceKind kindA, SurfaceKind kindB, const Face& faceA, const Face& faceB,
    const BooleanContext& ctx)
{
  if (kindA == SurfaceKind::Plane && kindB == SurfaceKind::Plane)
  {
    return IntersectPlanePlaneFaces(faceA, faceB, ctx);
  }
  if (kindA == SurfaceKind::Plane && kindB == SurfaceKind::Sphere)
  {
    return IntersectPlaneSphereFaces(faceA, faceB, ctx);
  }
  if (kindA == SurfaceKind::Sphere && kindB == SurfaceKind::Plane)
  {
    return IntersectPlaneSphereFaces(faceB, faceA, ctx);
  }
  return {};
}

void AppendAnalyticCircle(const Face& faceA, const Face& faceB,
                          const BooleanContext& ctx,
                          IntersectionGraph& graph)
{
    const SurfaceKind kindA = faceA.Surface->Kind();
    const SurfaceKind kindB = faceB.Surface->Kind();
    if (kindA == SurfaceKind::Sphere && kindB == SurfaceKind::Sphere)
    {
        const auto& sphereA =
            static_cast<const SphereSurface&>(*faceA.Surface);
        const auto& sphereB =
            static_cast<const SphereSurface&>(*faceB.Surface);
        const SphereSphereResult circle =
            IntersectSphereSphere(sphereA, sphereB, ctx);
        if (circle.IsCircle())
        {
            graph.Circles.push_back(
                IntersectionCircle{circle.Center, circle.Normal, circle.Radius,
                                   const_cast<Face*>(&faceA),
                                   const_cast<Face*>(&faceB), true});
        }
        return;
    }

    const Face* planeFace = nullptr;
    const Face* sphereFace = nullptr;
    if (kindA == SurfaceKind::Plane && kindB == SurfaceKind::Sphere)
    {
        planeFace = &faceA;
        sphereFace = &faceB;
    }
    else if (kindA == SurfaceKind::Sphere && kindB == SurfaceKind::Plane)
    {
        planeFace = &faceB;
        sphereFace = &faceA;
    }
    if (planeFace == nullptr || sphereFace == nullptr)
    {
        return;
    }

    const auto& plane =
        static_cast<const PlaneSurface&>(*planeFace->Surface);
    const auto& sphere =
        static_cast<const SphereSurface&>(*sphereFace->Surface);
    const PlaneSphereResult circle = IntersectPlaneSphere(plane, sphere, ctx);
    if (circle.IsCircle())
    {
        graph.Circles.push_back(
            IntersectionCircle{circle.Center, circle.Normal, circle.Radius,
                               const_cast<Face*>(planeFace),
                               const_cast<Face*>(sphereFace), false});
    }
}

}  // namespace

IntersectProbe ProbeAnalyticSurfacePair(const Face& faceA, const Face& faceB,
                                        const BooleanContext& ctx)
{
  if (!faceA.Surface || !faceB.Surface)
  {
    return IntersectProbe::Unsupported;
  }
  return Dispatch(faceA.Surface->Kind(), faceB.Surface->Kind(), faceA, faceB,
                  ctx);
}

IntersectorRegistry::IntersectorRegistry() = default;

IntersectProbe IntersectorRegistry::Probe(const Face& faceA, const Face& faceB,
                                          const BooleanContext& ctx) const
{
  return ProbeAnalyticSurfacePair(faceA, faceB, ctx);
}

std::vector<IntersectionSegment> IntersectorRegistry::Intersect(
    const Face& faceA, const Face& faceB, const BooleanContext& ctx) const
{
  if (!faceA.Surface || !faceB.Surface)
  {
    return {};
  }
  return IntersectFaces(faceA.Surface->Kind(), faceB.Surface->Kind(), faceA, faceB,
                        ctx);
}

IntersectionGraph IntersectorRegistry::BuildGraph(
    const Body& bodyA, const Body& bodyB, const BooleanContext& ctx,
    spatial::BuildQuality quality) const
{
  IntersectionGraph graph;
  const std::vector<FacePairCandidate> candidates =
      CollectFacePairCandidates(bodyA, bodyB, quality);
  for (const FacePairCandidate& candidate : candidates)
  {
    if (!candidate.a || !candidate.b)
    {
      continue;
    }
    std::vector<IntersectionSegment> segments =
        Intersect(*candidate.a, *candidate.b, ctx);
    graph.Segments.insert(graph.Segments.end(),
                          std::make_move_iterator(segments.begin()),
                          std::make_move_iterator(segments.end()));
    AppendAnalyticCircle(*candidate.a, *candidate.b, ctx, graph);
  }
  return graph;
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
