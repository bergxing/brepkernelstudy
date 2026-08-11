#include "brep/bool/broadphase.hpp"

#include "brep/bool/intersect_plane_cylinder.hpp"
#include "brep/bool/intersect_plane_plane.hpp"
#include "brep/bool/intersect_plane_sphere.hpp"
#include "brep/bool/intersect_sphere_cylinder.hpp"
#include "brep/bool/intersect_sphere_sphere.hpp"
#include "brep/geometry.hpp"

#include <cstdint>
#include <sstream>
#include <utility>

namespace brep::boolean {
namespace {

enum class PairProbe : std::uint8_t { Nonempty, Empty, Unsupported };

[[nodiscard]] PairProbe probe_one_pair(const Face& fa, const Face& fb,
                                       const BooleanContext& ctx) {
  if (!fa.surface || !fb.surface) return PairProbe::Unsupported;

  const SurfaceKind ka = fa.surface->kind();
  const SurfaceKind kb = fb.surface->kind();

  auto plane_plane = [&](const PlaneSurface& a, const PlaneSurface& b) {
    const auto r = intersect_plane_plane(a, b, ctx);
    return r.status == PlanePlaneStatus::Line ||
                   r.status == PlanePlaneStatus::Coincident
               ? PairProbe::Nonempty
               : PairProbe::Empty;
  };
  auto plane_sphere = [&](const PlaneSurface& p, const SphereSurface& s) {
    const auto r = intersect_plane_sphere(p, s, ctx);
    return r.status == PlaneSphereStatus::Empty ? PairProbe::Empty
                                                : PairProbe::Nonempty;
  };
  auto sphere_sphere = [&](const SphereSurface& a, const SphereSurface& b) {
    const auto r = intersect_sphere_sphere(a, b, ctx);
    return (r.status == SphereSphereStatus::Separate ||
            r.status == SphereSphereStatus::Contained)
               ? PairProbe::Empty
               : PairProbe::Nonempty;
  };
  auto plane_cyl = [&](const PlaneSurface& p, const CylinderSurface& c) {
    const auto r = intersect_plane_cylinder(p, c, ctx);
    return r.status == PlaneCylinderStatus::Empty ? PairProbe::Empty
                                                  : PairProbe::Nonempty;
  };
  auto sphere_cyl = [&](const SphereSurface& s, const CylinderSurface& c) {
    const auto r = intersect_sphere_cylinder(s, c, ctx);
    if (r.status == SphereCylinderStatus::Unsupported) {
      return PairProbe::Unsupported;
    }
    return r.status == SphereCylinderStatus::Empty ? PairProbe::Empty
                                                   : PairProbe::Nonempty;
  };

  if (ka == SurfaceKind::Plane && kb == SurfaceKind::Plane) {
    return plane_plane(static_cast<const PlaneSurface&>(*fa.surface),
                       static_cast<const PlaneSurface&>(*fb.surface));
  }
  if (ka == SurfaceKind::Plane && kb == SurfaceKind::Sphere) {
    return plane_sphere(static_cast<const PlaneSurface&>(*fa.surface),
                        static_cast<const SphereSurface&>(*fb.surface));
  }
  if (ka == SurfaceKind::Sphere && kb == SurfaceKind::Plane) {
    return plane_sphere(static_cast<const PlaneSurface&>(*fb.surface),
                        static_cast<const SphereSurface&>(*fa.surface));
  }
  if (ka == SurfaceKind::Sphere && kb == SurfaceKind::Sphere) {
    return sphere_sphere(static_cast<const SphereSurface&>(*fa.surface),
                         static_cast<const SphereSurface&>(*fb.surface));
  }
  if (ka == SurfaceKind::Plane && kb == SurfaceKind::Cylinder) {
    return plane_cyl(static_cast<const PlaneSurface&>(*fa.surface),
                     static_cast<const CylinderSurface&>(*fb.surface));
  }
  if (ka == SurfaceKind::Cylinder && kb == SurfaceKind::Plane) {
    return plane_cyl(static_cast<const PlaneSurface&>(*fb.surface),
                     static_cast<const CylinderSurface&>(*fa.surface));
  }
  if (ka == SurfaceKind::Sphere && kb == SurfaceKind::Cylinder) {
    return sphere_cyl(static_cast<const SphereSurface&>(*fa.surface),
                      static_cast<const CylinderSurface&>(*fb.surface));
  }
  if (ka == SurfaceKind::Cylinder && kb == SurfaceKind::Sphere) {
    return sphere_cyl(static_cast<const SphereSurface&>(*fb.surface),
                      static_cast<const CylinderSurface&>(*fa.surface));
  }
  return PairProbe::Unsupported;
}

}  // namespace

std::vector<FacePairCandidate> collect_face_pair_candidates(
    const Body& a, const Body& b, spatial::BuildQuality quality) {
  const spatial::FaceBvh bvh_a = spatial::FaceBvh::build(a, quality);
  const spatial::FaceBvh bvh_b = spatial::FaceBvh::build(b, quality);
  const auto raw = spatial::FaceBvh::candidate_pairs(bvh_a, bvh_b);
  std::vector<FacePairCandidate> out;
  out.reserve(raw.size());
  for (const auto& p : raw) {
    out.push_back(FacePairCandidate{p.first, p.second});
  }
  return out;
}

BroadphaseProbe probe_face_pair_intersections(const Body& a, const Body& b,
                                              spatial::BuildQuality quality,
                                              const BooleanContext& ctx) {
  BroadphaseProbe probe;
  probe.quality = quality;
  const auto candidates = collect_face_pair_candidates(a, b, quality);
  probe.candidate_pairs = candidates.size();

  for (const FacePairCandidate& c : candidates) {
    if (!c.a || !c.b) {
      ++probe.unsupported_pairs;
      continue;
    }
    ++probe.attempted_intersects;
    switch (probe_one_pair(*c.a, *c.b, ctx)) {
      case PairProbe::Nonempty:
        ++probe.nonempty_intersects;
        break;
      case PairProbe::Empty:
        break;
      case PairProbe::Unsupported:
        ++probe.unsupported_pairs;
        break;
    }
  }

  const char* qname =
      quality == spatial::BuildQuality::Sah ? "Sah" : "Median";
  std::ostringstream oss;
  oss << "broadphase(" << qname << "): " << probe.candidate_pairs
      << " candidate face pairs, " << probe.nonempty_intersects
      << " nonempty analytic intersects, " << probe.unsupported_pairs
      << " unsupported (imprint/classify not wired)";
  probe.summary = oss.str();
  return probe;
}

}  // namespace brep::boolean
