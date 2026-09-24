#pragma once

#include "brep/bool/Context.h"
#include "brep/Math.h"
#include "brep/Types.h"
#include "brep/spatial/FaceBvh.h"
#include "brep/Topology.h"

#include <optional>
#include <utility>
#include <vector>

namespace brep::boolean
{

/// A bounded 3D intersection segment between two faces (Intersect stage output).
struct IntersectionSegment
{
  Point3d Start{};
  Point3d End{};
  Face* FaceA{nullptr};
  Face* FaceB{nullptr};
  CurveKind CurveKind{CurveKind::Line};
};

struct IntersectionCircle
{
    Point3d Center{};
    Vector3d Normal{};
    double Radius{0.0};
    Face* FaceA{nullptr};
    Face* FaceB{nullptr};
    bool Closed{false};
};

/// Face piece after Imprint (placeholder; topological split expands later).
struct FaceFragment
{
  Face* SourceFace{nullptr};
};

struct IntersectionGraph
{
  std::vector<IntersectionSegment> Segments;
    std::vector<IntersectionCircle> Circles;
  std::vector<FaceFragment> Fragments;
};

/// Clip an infinite line to a planar face's outer loop (convex polygon).
[[nodiscard]] std::optional<std::pair<double, double>> ClipLineToPlanarFace(
    const Face& face, const Point3d& lineOrigin, const Vector3d& lineDirection,
    double eps);

/// Plane–plane face pair: infinite line clipped to both face boundaries.
[[nodiscard]] std::vector<IntersectionSegment> IntersectPlanePlaneFaces(
    const Face& faceA, const Face& faceB, const BooleanContext& ctx);

/// Plane–sphere face pair: circle tessellated into in-face chord segments.
[[nodiscard]] std::vector<IntersectionSegment> IntersectPlaneSphereFaces(
    const Face& facePlane, const Face& faceSphere, const BooleanContext& ctx);

/// Collect intersection segments for all broadphase candidate face pairs.
[[nodiscard]] IntersectionGraph BuildIntersectionGraph(
    const Body& bodyA, const Body& bodyB, const BooleanContext& ctx = {},
    spatial::BuildQuality quality = spatial::BuildQuality::Sah);

}  // namespace brep::boolean
