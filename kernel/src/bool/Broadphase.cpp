#include "brep/bool/Broadphase.h"

#include "brep/bool/IntersectPlaneCylinder.h"
#include "brep/bool/IntersectPlanePlane.h"
#include "brep/bool/IntersectPlaneSphere.h"
#include "brep/bool/IntersectSphereCylinder.h"
#include "brep/bool/IntersectSphereSphere.h"
#include "brep/Geometry.h"

#include <cstdint>
#include <sstream>
#include <utility>

namespace brep::boolean
{
namespace
{

enum class PairProbe : std::uint8_t
{
    Nonempty, Empty, Unsupported 
};

[[nodiscard]] PairProbe probe_one_pair(const Face& fa, const Face& fb,
                                       const BooleanContext& ctx)
{
  if (!fa.Surface || !fb.Surface) return PairProbe::Unsupported;

  const SurfaceKind ka = fa.Surface->Kind();
  const SurfaceKind kb = fb.Surface->Kind();

  auto plane_plane = [&](const PlaneSurface& a, const PlaneSurface& b)
  {
    const auto r = IntersectPlanePlane(a, b, ctx);
    return r.status == PlanePlaneStatus::Line ||
                   r.status == PlanePlaneStatus::Coincident
               ? PairProbe::Nonempty
               : PairProbe::Empty;
  };
  auto plane_sphere = [&](const PlaneSurface& p, const SphereSurface& s)
  {
    const auto r = IntersectPlaneSphere(p, s, ctx);
    return r.status == PlaneSphereStatus::Empty ? PairProbe::Empty
                                                : PairProbe::Nonempty;
  };
  auto sphere_sphere = [&](const SphereSurface& a, const SphereSurface& b)
  {
    const auto r = IntersectSphereSphere(a, b, ctx);
    return (r.status == SphereSphereStatus::Separate ||
            r.status == SphereSphereStatus::Contained)
               ? PairProbe::Empty
               : PairProbe::Nonempty;
  };
  auto plane_cyl = [&](const PlaneSurface& p, const CylinderSurface& c)
  {
    const auto r = IntersectPlaneCylinder(p, c, ctx);
    return r.status == PlaneCylinderStatus::Empty ? PairProbe::Empty
                                                  : PairProbe::Nonempty;
  };
  auto sphere_cyl = [&](const SphereSurface& s, const CylinderSurface& c)
  {
    const auto r = IntersectSphereCylinder(s, c, ctx);
    if (r.status == SphereCylinderStatus::Unsupported)
    {
      return PairProbe::Unsupported;
    }
    return r.status == SphereCylinderStatus::Empty ? PairProbe::Empty
                                                   : PairProbe::Nonempty;
  };

  if (ka == SurfaceKind::Plane && kb == SurfaceKind::Plane)
  {
    return plane_plane(static_cast<const PlaneSurface&>(*fa.Surface),
                       static_cast<const PlaneSurface&>(*fb.Surface));
  }
  if (ka == SurfaceKind::Plane && kb == SurfaceKind::Sphere)
  {
    return plane_sphere(static_cast<const PlaneSurface&>(*fa.Surface),
                        static_cast<const SphereSurface&>(*fb.Surface));
  }
  if (ka == SurfaceKind::Sphere && kb == SurfaceKind::Plane)
  {
    return plane_sphere(static_cast<const PlaneSurface&>(*fb.Surface),
                        static_cast<const SphereSurface&>(*fa.Surface));
  }
  if (ka == SurfaceKind::Sphere && kb == SurfaceKind::Sphere)
  {
    return sphere_sphere(static_cast<const SphereSurface&>(*fa.Surface),
                         static_cast<const SphereSurface&>(*fb.Surface));
  }
  if (ka == SurfaceKind::Plane && kb == SurfaceKind::Cylinder)
  {
    return plane_cyl(static_cast<const PlaneSurface&>(*fa.Surface),
                     static_cast<const CylinderSurface&>(*fb.Surface));
  }
  if (ka == SurfaceKind::Cylinder && kb == SurfaceKind::Plane)
  {
    return plane_cyl(static_cast<const PlaneSurface&>(*fb.Surface),
                     static_cast<const CylinderSurface&>(*fa.Surface));
  }
  if (ka == SurfaceKind::Sphere && kb == SurfaceKind::Cylinder)
  {
    return sphere_cyl(static_cast<const SphereSurface&>(*fa.Surface),
                      static_cast<const CylinderSurface&>(*fb.Surface));
  }
  if (ka == SurfaceKind::Cylinder && kb == SurfaceKind::Sphere)
  {
    return sphere_cyl(static_cast<const SphereSurface&>(*fb.Surface),
                      static_cast<const CylinderSurface&>(*fa.Surface));
  }
  return PairProbe::Unsupported;
}

}  // namespace

std::vector<FacePairCandidate> CollectFacePairCandidates(
    const Body& a, const Body& b, spatial::BuildQuality quality)
{
  const spatial::FaceBvh bvh_a = spatial::FaceBvh::Build(a, quality);
  const spatial::FaceBvh bvh_b = spatial::FaceBvh::Build(b, quality);
  const auto raw = spatial::FaceBvh::CandidatePairs(bvh_a, bvh_b);
  std::vector<FacePairCandidate> out;
  out.reserve(raw.size());
  for (const auto& p : raw)
  {
    out.push_back(FacePairCandidate{p.first, p.second});
  }
  return out;
}

BroadphaseProbe ProbeFacePairIntersections(const Body& a, const Body& b,
                                              spatial::BuildQuality quality,
                                              const BooleanContext& ctx)
                                              {
  BroadphaseProbe probe;
  probe.Quality = quality;
  const auto candidates = CollectFacePairCandidates(a, b, quality);
  probe.CandidatePairs = candidates.size();

  for (const FacePairCandidate& c : candidates)
  {
    if (!c.a || !c.b)
  {
      ++probe.UnsupportedPairs;
      continue;
    }
    ++probe.AttemptedIntersects;
    switch (probe_one_pair(*c.a, *c.b, ctx))
    {
      case PairProbe::Nonempty:
        ++probe.NonemptyIntersects;
        break;
      case PairProbe::Empty:
        break;
      case PairProbe::Unsupported:
        ++probe.UnsupportedPairs;
        break;
    }
  }

  const char* qname =
      quality == spatial::BuildQuality::Sah ? "Sah" : "Median";
  std::ostringstream oss;
  oss << "broadphase(" << qname << "): " << probe.CandidatePairs
      << " candidate face pairs, " << probe.NonemptyIntersects
      << " nonempty analytic intersects, " << probe.UnsupportedPairs
      << " unsupported (imprint/classify not wired)";
  probe.Summary = oss.str();
  return probe;
}

}  // namespace brep::boolean
