#include "brep/bool/Broadphase.h"

#include "brep/bool/IntersectorRegistry.h"

#include <sstream>

namespace brep::boolean
{

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
    switch (ProbeAnalyticSurfacePair(*c.a, *c.b, ctx))
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
