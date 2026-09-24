#include "brep/bool/Pipeline.h"

#include "brep/Aspect.h"
#include "brep/bool/BooleanBuilder.h"
#include "brep/bool/ImprintEngine.h"
#include "brep/bool/IntersectorRegistry.h"
#include "brep/bool/SolidClassifier.h"
#include "brep/bool/TopologyCopy.h"
#include "brep/Log.h"
#include "brep/spatial/FaceBvh.h"
#include <algorithm>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace brep::boolean
{
namespace
{

[[nodiscard]] const char* OpName(BooleanOp op) noexcept
{
  switch (op)
  {
  case BooleanOp::Union:
    return "Union";
  case BooleanOp::Subtract:
    return "Subtract";
  case BooleanOp::Intersect:
    return "Intersect";
  }
  return "Unknown";
}

[[nodiscard]] std::size_t CountFaces(const Body& body)
{
    std::size_t faceCount = 0;
    for (const Shell* shell : body.Shells)
    {
        if (!shell)
    {
            continue;
        }
        faceCount += shell->Faces.size();
    }
    return faceCount;
}

[[nodiscard]] bool HasPositiveAabbOverlap(const Body& a, const Body& b,
                                          double eps)
{
    const spatial::Aabb boundsA =
        spatial::FaceBvh::Build(a, spatial::BuildQuality::Sah).RootBounds();
    const spatial::Aabb boundsB =
        spatial::FaceBvh::Build(b, spatial::BuildQuality::Sah).RootBounds();
    return std::min(boundsA.Max.x(), boundsB.Max.x()) -
                   std::max(boundsA.Min.x(), boundsB.Min.x()) >
               eps &&
           std::min(boundsA.Max.y(), boundsB.Max.y()) -
                   std::max(boundsA.Min.y(), boundsB.Min.y()) >
               eps &&
           std::min(boundsA.Max.z(), boundsB.Max.z()) -
                   std::max(boundsA.Min.z(), boundsB.Min.z()) >
               eps;
}

[[nodiscard]] Point3d PointOnSphere(const SphereSurface& sphere,
                                    const Point3d& candidate)
{
    const Vector3d dir = candidate - sphere.Center();
    if (dir.norm() <= 1e-9)
    {
        return sphere.Eval(0.25, 0.25);
    }
    return sphere.Center() + dir.normalized() * sphere.Radius();
}

[[nodiscard]] Point3d FaceSamplePoint(const Face& face)
{
    if (!face.Surface)
    {
        return {};
    }
    if (face.Surface->Kind() == SurfaceKind::Sphere)
    {
        const auto& sphere =
            static_cast<const SphereSurface&>(*face.Surface);
        for (const Loop* loop : face.Loops)
        {
            if (loop == nullptr || loop->First == nullptr ||
                loop->First->Edge == nullptr ||
                loop->First->Edge->Curve == nullptr ||
                loop->First->Edge->Curve->Kind() != CurveKind::Circle ||
                loop->First->Edge->Name.find("_imprint") == std::string::npos)
            {
                continue;
            }
            const auto& circle =
                static_cast<const CircleCurve&>(*loop->First->Edge->Curve);
            const double side =
                loop->Type == LoopType::Inner ? -1.0 : 1.0;
            const Vector3d offset = circle.Center() - sphere.Center();
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
                    // A 2-arc lune shares both poles; the vertex sum
                    // cancels. Arc midpoints point into the polygon.
                    if (coedge.Edge != nullptr &&
                        coedge.Edge->Curve != nullptr)
                    {
                        const Point3d midpoint = coedge.Edge->Curve->Eval(
                            0.5 * (coedge.Edge->T0 + coedge.Edge->T1));
                        direction += midpoint - sphere.Center();
                    }
                });
                // Multi-arc patches (octant, vertex nick): the vertex
                // + midpoint sum points into the spherical polygon.
                if (direction.norm() > 1e-9 &&
                    (offset.norm() > 1e-9 || seen.size() >= 2U ||
                     std::abs(direction.normalized().dot(circle.Normal())) >
                         0.25))
                {
                    return sphere.Center() +
                           direction.normalized() *
                               (side * sphere.Radius());
                }
            }
            // Isolated great-circle cap/remainder: use the circle normal.
            if (offset.norm() <= 1e-9)
            {
                return sphere.Center() +
                       circle.Normal() * (side * sphere.Radius());
            }
            return sphere.Center() +
                   circle.Normal() * (side * sphere.Radius());
        }
    }
    const Loop* outer = face.OuterLoop();
    if (!outer || !outer->First)
    {
        return face.Surface->Eval(0.25, 0.25);
    }
    Vector3d sum;
    std::size_t count = 0;
    outer->ForEachCoedge([&](const CoEdge& coedge)
    {
        if (coedge.From())
        {
            const Point3d position = coedge.From()->Position();
            sum += Vector3d{position.x(), position.y(), position.z()};
            ++count;
        }
        if (coedge.Edge != nullptr && coedge.Edge->Curve != nullptr)
        {
            const Point3d midpoint = coedge.Edge->Curve->Eval(
                0.5 * (coedge.Edge->T0 + coedge.Edge->T1));
            sum += Vector3d{midpoint.x(), midpoint.y(), midpoint.z()};
            ++count;
        }
    });
    if (count == 0)
    {
        return face.Surface->Eval(0.25, 0.25);
    }
    const Vector3d average = sum / static_cast<double>(count);
    const Point3d sample{average.x(), average.y(), average.z()};
    // A whole-sphere outer loop is two poles + a seam. Their average
    // sits near the center, inside the ball — not on the face.
    if (face.Surface->Kind() == SurfaceKind::Sphere)
    {
        return PointOnSphere(
            static_cast<const SphereSurface&>(*face.Surface), sample);
    }
    return sample;
}

[[nodiscard]] const CircleCurve* FirstImprintCircle(const Loop& loop)
{
    const CoEdge* coedge = loop.First;
    std::size_t guard = 0;
    while (coedge != nullptr && guard++ < 1024U)
    {
        if (coedge->Edge != nullptr && coedge->Edge->Curve != nullptr &&
            coedge->Edge->Curve->Kind() == CurveKind::Circle &&
            coedge->Edge->Name.find("_imprint") != std::string::npos)
        {
            return static_cast<const CircleCurve*>(coedge->Edge->Curve);
        }
        coedge = coedge->Next;
        if (coedge == loop.First)
        {
            break;
        }
    }
    return nullptr;
}

[[nodiscard]] bool SphereDirInSmallCap(const SphereSurface& sphere,
                                       const CircleCurve& circle,
                                       const Vector3d& dir, double eps)
{
    const Vector3d offset = circle.Center() - sphere.Center();
    if (offset.norm() <= eps)
    {
        return dir.dot(circle.Normal()) > 0.0;
    }
    const double cosAlpha = offset.norm() / sphere.Radius();
    return dir.dot(offset.normalized()) > cosAlpha - 1e-9;
}

// Small cap: toward the circle center. Large leftover: the opposite pole.
[[nodiscard]] Point3d SphereCirclePole(const SphereSurface& sphere,
                                       const CircleCurve& circle,
                                       bool smallCap)
{
    const Vector3d offset = circle.Center() - sphere.Center();
    Vector3d dir = offset.norm() > 1e-9 ? offset.normalized() : circle.Normal();
    if (!smallCap)
    {
        dir = -dir;
    }
    if (dir.norm() <= 1e-9)
    {
        return sphere.Center();
    }
    return sphere.Center() + dir.normalized() * sphere.Radius();
}

[[nodiscard]] const CircleCurve* FirstSphereCircle(const Face& face)
{
    for (const Loop* loop : face.Loops)
    {
        if (loop == nullptr)
        {
            continue;
        }
        const CircleCurve* circle = FirstImprintCircle(*loop);
        if (circle != nullptr)
        {
            return circle;
        }
    }
    return nullptr;
}

// Remainder is the sphere face after small caps are punched. Sample a
// point that is not inside any of those caps (the old 2-hole equator
// can land on a third cap when the sphere clips extra walls).
[[nodiscard]] Point3d SphereRemainderSamplePoint(const Face& face)
{
    if (face.Surface == nullptr ||
        face.Surface->Kind() != SurfaceKind::Sphere)
    {
        return FaceSamplePoint(face);
    }
    const auto& sphere = static_cast<const SphereSurface&>(*face.Surface);
    std::vector<const CircleCurve*> holes;
    Vector3d inward;
    for (const Loop* loop : face.Loops)
    {
        if (loop == nullptr || loop->Type != LoopType::Inner)
        {
            continue;
        }
        const CircleCurve* circle = FirstImprintCircle(*loop);
        if (circle == nullptr)
        {
            continue;
        }
        Vector3d axis = circle->Center() - sphere.Center();
        // Great-circle holes have no offset pole. A closed two-half
        // circle (top nick) must use the circle normal — the equator
        // midpoint sits On the cutting plane. Open-arc lunes use the
        // first-edge midpoint so the leftover points away from the slit.
        if (axis.norm() <= 1e-9)
        {
            int closedHalves = 0;
            loop->ForEachCoedge([&](const CoEdge& coedge)
            {
                if (coedge.Edge != nullptr &&
                    coedge.Edge->Name.find("_circle_imprint_edge") !=
                        std::string::npos)
                {
                    ++closedHalves;
                }
            });
            if (closedHalves < 2 && loop->First != nullptr &&
                loop->First->Edge != nullptr &&
                loop->First->Edge->Curve != nullptr)
            {
                const Edge& holeEdge = *loop->First->Edge;
                axis = holeEdge.Curve->Eval(
                           0.5 * (holeEdge.T0 + holeEdge.T1)) -
                       sphere.Center();
            }
            if (axis.norm() <= 1e-9)
            {
                axis = circle->Normal();
            }
        }
        holes.push_back(circle);
        inward -= axis.normalized();
    }
    if (holes.empty())
    {
        return FaceSamplePoint(face);
    }

    std::vector<Vector3d> candidates;
    if (inward.norm() > 1e-9)
    {
        candidates.push_back(inward.normalized());
    }
    if (holes.size() >= 2U)
    {
        Vector3d axis = holes.front()->Center() - sphere.Center();
        if (axis.norm() <= 1e-9)
        {
            axis = holes.front()->Normal();
        }
        if (axis.norm() > 1e-9)
        {
            Vector3d perp = axis.normalized().cross(Vector3d{0.0, 0.0, 1.0});
            if (perp.norm() <= 1e-9)
            {
                perp = axis.normalized().cross(Vector3d{1.0, 0.0, 0.0});
            }
            if (perp.norm() > 1e-9)
            {
                candidates.push_back(perp.normalized());
            }
        }
    }
    for (const CircleCurve* circle : holes)
    {
        Vector3d axis = circle->Center() - sphere.Center();
        if (axis.norm() <= 1e-9)
        {
            axis = circle->Normal();
        }
        if (axis.norm() > 1e-9)
        {
            candidates.push_back(-axis.normalized());
        }
    }

    const auto onRemainder = [&](const Vector3d& dir) -> bool
    {
        for (const CircleCurve* circle : holes)
        {
            if (SphereDirInSmallCap(sphere, *circle, dir, 1e-9))
            {
                return false;
            }
        }
        return true;
    };
    for (const Vector3d& dir : candidates)
    {
        if (onRemainder(dir))
        {
            return sphere.Center() + dir * sphere.Radius();
        }
    }
    return FaceSamplePoint(face);
}

[[nodiscard]] std::optional<Point3d> CircleImprintCenter(const Face& face)
{
    for (const Loop* loop : face.Loops)
    {
        if (loop == nullptr || loop->First == nullptr)
        {
            continue;
        }
        CoEdge* coedge = loop->First;
        std::size_t guard = 0;
        do
        {
            if (++guard > 1024U)
            {
                break;
            }
            if (coedge->Edge != nullptr && coedge->Edge->Curve != nullptr &&
                coedge->Edge->Curve->Kind() == CurveKind::Circle &&
                coedge->Edge->Name.find("_imprint") != std::string::npos)
            {
                const auto& circle =
                    static_cast<const CircleCurve&>(*coedge->Edge->Curve);
                return circle.Center();
            }
            coedge = coedge->Next;
        } while (coedge != nullptr && coedge != loop->First);
    }
    return std::nullopt;
}

[[nodiscard]] Point3d RingFaceSamplePoint(const Face& face, double eps)
{
    if (face.Surface == nullptr ||
        face.Surface->Kind() != SurfaceKind::Plane)
    {
        return FaceSamplePoint(face);
    }
    const Loop* innerCircle = nullptr;
    for (const Loop* loop : face.Loops)
    {
        if (loop == nullptr || loop->Type != LoopType::Inner ||
            loop->First == nullptr || loop->First->Edge == nullptr ||
            loop->First->Edge->Curve == nullptr ||
            loop->First->Edge->Curve->Kind() != CurveKind::Circle ||
            loop->First->Edge->Name.find("_imprint") == std::string::npos)
        {
            continue;
        }
        innerCircle = loop;
        break;
    }
    if (innerCircle == nullptr)
    {
        return FaceSamplePoint(face);
    }
    const auto& circle =
        static_cast<const CircleCurve&>(*innerCircle->First->Edge->Curve);
    const Point3d center = circle.Center();
    const Loop* outer = face.OuterLoop();
    Vector3d bestRadial;
    double bestDist = -1.0;
    if (outer != nullptr)
    {
        outer->ForEachCoedge([&](const CoEdge& coedge)
        {
            if (coedge.From() == nullptr)
            {
                return;
            }
            Vector3d radial{coedge.From()->Position().x() - center.x(),
                            coedge.From()->Position().y() - center.y(),
                            coedge.From()->Position().z() - center.z()};
            radial -= circle.Normal() * circle.Normal().dot(radial);
            const double dist = radial.norm();
            if (dist > bestDist)
            {
                bestDist = dist;
                bestRadial = radial;
            }
        });
    }
    if (bestDist <= circle.Radius() + eps || bestRadial.norm() <= eps)
    {
        return FaceSamplePoint(face);
    }
    const double sampleRadius = 0.5 * (bestDist + circle.Radius());
    const Vector3d unit = bestRadial.normalized();
    return Point3d{center.x() + unit.x() * sampleRadius,
                   center.y() + unit.y() * sampleRadius,
                   center.z() + unit.z() * sampleRadius};
}

[[nodiscard]] bool HasImprintFragmentSibling(const Face& face, const Body& owner)
{
    if (face.Name.find("_split") != std::string::npos ||
        face.Name.find("_sphere_imprint_patch") != std::string::npos ||
        face.Name.find("_circle_imprint_disk") != std::string::npos)
    {
        return false;
    }
    for (const Shell* shell : owner.Shells)
    {
        if (shell == nullptr)
        {
            continue;
        }
        for (const Face* candidate : shell->Faces)
        {
            if (candidate == nullptr || candidate == &face)
            {
                continue;
            }
            if (candidate->Name.rfind(face.Name, 0) != 0U)
            {
                continue;
            }
            if (candidate->Name.find("_sphere_imprint_patch") !=
                    std::string::npos ||
                candidate->Name.find("_circle_imprint_disk") !=
                    std::string::npos)
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool HasSplitFragmentSibling(const Face& face, const Body& owner)
{
    if (face.Name.find("_split") != std::string::npos)
    {
        return false;
    }
    for (const Shell* shell : owner.Shells)
    {
        if (shell == nullptr)
        {
            continue;
        }
        for (const Face* candidate : shell->Faces)
        {
            if (candidate == nullptr || candidate == &face)
            {
                continue;
            }
            if (candidate->Name.rfind(face.Name, 0) != 0U)
            {
                continue;
            }
            if (candidate->Name.find("_split") != std::string::npos)
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool HasComplementInnerLoop(const Face& face)
{
    for (const Loop* loop : face.Loops)
    {
        if (loop != nullptr &&
            loop->Name.find("_complement_inner") != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] FaceRegion ClassifyFaceAgainstBodyImpl(const Face& face,
                                                 const Body& solid,
                                                 const Body& owner,
                                                 double eps,
                                                 BooleanOp op,
                                                 bool fromA)
{
    (void)fromA;
    // Sphere (or planar) remainder punched by a closed imprint circle.
    // The circle center lies on the cutting plane and must not drive
    // classification — use a point on the annulus/remainder instead.
    if (HasComplementInnerLoop(face))
    {
        bool closedCircleHole = false;
        for (const Loop* loop : face.Loops)
        {
            if (loop == nullptr || loop->Type != LoopType::Inner)
            {
                continue;
            }
            int closedHalves = 0;
            loop->ForEachCoedge([&](const CoEdge& coedge)
            {
                if (coedge.Edge != nullptr &&
                    coedge.Edge->Name.find("_circle_imprint_edge") !=
                        std::string::npos)
                {
                    ++closedHalves;
                }
            });
            if (closedHalves >= 2)
            {
                closedCircleHole = true;
                break;
            }
        }
        if (!closedCircleHole)
        {
            if (op == BooleanOp::Subtract)
            {
                // Open-arc nick leftover is the sphere outside the solid.
                return FaceRegion::Out;
            }
        }
        else
        {
            const SolidClass classification = ClassifyPointInBody(
                solid, SphereRemainderSamplePoint(face), eps);
            if (op == BooleanOp::Union)
            {
                if (classification == SolidClass::Out)
                {
                    return FaceRegion::Out;
                }
                // The sample can fall in the punched cap while the annulus
                // still bulges outside the partner (pad+sphere union).
                const Loop* outer = face.OuterLoop();
                if (outer != nullptr)
                {
                    int outsideVertices = 0;
                    int insideVertices = 0;
                    std::unordered_set<const Vertex*> seen;
                    outer->ForEachCoedge([&](const CoEdge& coedge)
                    {
                        const Vertex* vertex = coedge.From();
                        if (vertex == nullptr || !seen.insert(vertex).second)
                        {
                            return;
                        }
                        const SolidClass vertexClass = ClassifyPointInBody(
                            solid, vertex->Position(), eps);
                        outsideVertices +=
                            vertexClass == SolidClass::Out ? 1 : 0;
                        insideVertices +=
                            vertexClass == SolidClass::In ? 1 : 0;
                    });
                    if (outsideVertices > 0 && insideVertices == 0)
                    {
                        return FaceRegion::Out;
                    }
                    if (insideVertices > 0 && outsideVertices == 0)
                    {
                        return FaceRegion::In;
                    }
                }
                return SolidClassToFaceRegion(classification);
            }
            return classification == SolidClass::In ? FaceRegion::In
                                                    : FaceRegion::Out;
        }
    }
    if (op == BooleanOp::Intersect &&
        face.Name.find("_split") != std::string::npos)
    {
        const SolidClass classification =
            ClassifyPointInBody(solid, FaceSamplePoint(face), eps);
        return classification == SolidClass::Out ? FaceRegion::Out
                                                 : FaceRegion::In;
    }
    if (op == BooleanOp::Intersect && HasSplitFragmentSibling(face, owner))
    {
        for (const Shell* shell : owner.Shells)
        {
            if (shell == nullptr)
            {
                continue;
            }
            for (const Face* candidate : shell->Faces)
            {
                if (candidate == nullptr || candidate == &face)
                {
                    continue;
                }
                if (candidate->Name.rfind(face.Name, 0) != 0U)
                {
                    continue;
                }
                if (candidate->Name.find("_split") == std::string::npos)
                {
                    continue;
                }
                if (ClassifyPointInBody(solid, FaceSamplePoint(*candidate),
                                        eps) != SolidClass::Out)
                {
                    return FaceRegion::Out;
                }
            }
        }
        const SolidClass classification =
            ClassifyPointInBody(solid, FaceSamplePoint(face), eps);
        return classification == SolidClass::In ? FaceRegion::In
                                                : FaceRegion::Out;
    }
    if (op == BooleanOp::Intersect && HasImprintFragmentSibling(face, owner))
    {
        return FaceRegion::Out;
    }
    if (face.Name.find("_sphere_imprint_patch") != std::string::npos)
    {
        const SolidClass classification =
            ClassifyPointInBody(solid, FaceSamplePoint(face), eps);
        if (op == BooleanOp::Subtract)
        {
            return classification == SolidClass::Out ? FaceRegion::Out
                                                     : FaceRegion::In;
        }
        return SolidClassToFaceRegion(classification);
    }
    if (op == BooleanOp::Union || op == BooleanOp::Subtract ||
        op == BooleanOp::Intersect)
    {
        if (op != BooleanOp::Intersect &&
            face.Name.find("_circle_imprint_disk") != std::string::npos)
        {
            return FaceRegion::In;
        }
        if (face.Name.find("_circle_imprint_cap") != std::string::npos)
        {
            Point3d sample = FaceSamplePoint(face);
            if (face.Surface != nullptr &&
                face.Surface->Kind() == SurfaceKind::Sphere)
            {
                if (const CircleCurve* circle = FirstSphereCircle(face))
                {
                    sample = SphereCirclePole(
                        static_cast<const SphereSurface&>(*face.Surface),
                        *circle, true);
                }
            }
            const SolidClass classification =
                ClassifyPointInBody(solid, sample, eps);
            if (op == BooleanOp::Subtract)
            {
                return classification == SolidClass::Out ? FaceRegion::Out
                                                         : FaceRegion::In;
            }
            return SolidClassToFaceRegion(classification);
        }
    }
    int insideVertices = 0;
    int outsideVertices = 0;
    bool hasCircleBoundary = false;
    bool hasInnerLoop = false;
    for (const Loop* loop : face.Loops)
    {
        if (loop != nullptr && loop->Type == LoopType::Inner)
        {
            hasInnerLoop = true;
            break;
        }
    }
    const Loop* outer = face.OuterLoop();
    std::size_t outerVertexCount = 0;
    if (outer != nullptr)
    {
        std::unordered_set<const Vertex*> seen;
        outer->ForEachCoedge([&](const CoEdge& coedge)
        {
            const Vertex* vertex = coedge.From();
            if (vertex == nullptr || !seen.insert(vertex).second)
            {
                return;
            }
            hasCircleBoundary =
                hasCircleBoundary ||
                (coedge.Edge != nullptr && coedge.Edge->Curve != nullptr &&
                 coedge.Edge->Curve->Kind() == CurveKind::Circle &&
                 coedge.Edge->Name.find("_imprint") != std::string::npos);
            const SolidClass classification =
                ClassifyPointInBody(solid, vertex->Position(), eps);
            insideVertices += classification == SolidClass::In ? 1 : 0;
            outsideVertices += classification == SolidClass::Out ? 1 : 0;
        });
        outerVertexCount = seen.size();
    }
    if (!hasCircleBoundary && !hasInnerLoop)
    {
        const SolidClass classification =
            ClassifyPointInBody(solid, FaceSamplePoint(face), eps);
        if (classification != SolidClass::On)
        {
            return SolidClassToFaceRegion(classification);
        }
        // Centroid can sit on a tangency point; fall through to vertices.
    }
    // Vertex-nick sliver: two poles lie on the intersection. Against the
    // imprinted partner they often classify Out, so the interior sample
    // is the only reliable vote. Skip closed disks/caps (two halves).
    int openImprintArcs = 0;
    if (outer != nullptr && hasCircleBoundary && outerVertexCount <= 2U)
    {
        outer->ForEachCoedge([&](const CoEdge& coedge)
        {
            if (coedge.Edge != nullptr &&
                coedge.Edge->Name.find("_imprint") != std::string::npos &&
                coedge.Edge->V0 != nullptr && coedge.Edge->V1 != nullptr &&
                coedge.Edge->V0 != coedge.Edge->V1)
            {
                ++openImprintArcs;
            }
        });
    }
    if (openImprintArcs == 1 &&
        face.Name.find("_circle_imprint_disk") == std::string::npos &&
        face.Name.find("_circle_imprint_cap") == std::string::npos)
    {
        const SolidClass classification =
            ClassifyPointInBody(solid, FaceSamplePoint(face), eps);
        if (classification != SolidClass::On)
        {
            return SolidClassToFaceRegion(classification);
        }
    }
    if (insideVertices > 0 && outsideVertices == 0)
    {
        return FaceRegion::In;
    }
    if (outsideVertices > 0 && insideVertices == 0)
    {
        const bool intersectImprintFace =
            op == BooleanOp::Intersect &&
            (hasCircleBoundary ||
             face.Name.find("_circle_imprint") != std::string::npos ||
             face.Name.find("_sphere_imprint_patch") != std::string::npos);
        if (!intersectImprintFace)
        {
            return FaceRegion::Out;
        }
        if (op == BooleanOp::Intersect && hasCircleBoundary)
        {
            const bool keptFragment =
                face.Name.find("_sphere_imprint_patch") != std::string::npos ||
                face.Name.find("_circle_imprint_disk") != std::string::npos;
            if (!keptFragment)
            {
                return FaceRegion::Out;
            }
        }
    }
    if (op == BooleanOp::Intersect && insideVertices > 0 && outsideVertices > 0)
    {
        const bool keepFragment =
            face.Name.find("_sphere_imprint_patch") != std::string::npos ||
            face.Name.find("_circle_imprint_disk") != std::string::npos;
        return keepFragment ? FaceRegion::In : FaceRegion::Out;
    }
    if (hasCircleBoundary && insideVertices != outsideVertices &&
        !(outsideVertices > 0 && insideVertices == 0))
    {
        return insideVertices > outsideVertices ? FaceRegion::In
                                                : FaceRegion::Out;
    }
    if (const std::optional<Point3d> imprintCenter = CircleImprintCenter(face))
    {
        return SolidClassToFaceRegion(
            ClassifyPointInBody(solid, *imprintCenter, eps));
    }
    const Point3d sample =
        hasInnerLoop ? RingFaceSamplePoint(face, eps) : FaceSamplePoint(face);
    return SolidClassToFaceRegion(ClassifyPointInBody(solid, sample, eps));
}

void CleanupWorkingBodies(PipelineState& state)
{
    if (state.TargetModel == nullptr)
    {
        return;
    }
    if (state.WorkingBodyA != nullptr && state.WorkingBodyA != state.BodyA)
    {
        state.TargetModel->RemoveBody(state.WorkingBodyA->Guid);
        state.WorkingBodyA = nullptr;
    }
    if (state.WorkingBodyB != nullptr && state.WorkingBodyB != state.BodyB)
    {
        state.TargetModel->RemoveBody(state.WorkingBodyB->Guid);
        state.WorkingBodyB = nullptr;
    }
}

class PreprocessStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Preprocess;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Preprocess;
        if (!state.BodyA || !state.BodyB || !state.TargetModel)
        {
            stageResult.Diagnostics = "pipeline Preprocess: missing operand or model";
            return stageResult;
        }
        if (state.BodyA->Shells.empty() || state.BodyB->Shells.empty())
        {
            stageResult.Diagnostics = "pipeline Preprocess: operand has no shells";
            return stageResult;
        }
        if (CountFaces(*state.BodyA) == 0 || CountFaces(*state.BodyB) == 0)
        {
            stageResult.Diagnostics = "pipeline Preprocess: operand has no faces";
            return stageResult;
        }
        TopologyCopyContext ctxA{*state.TargetModel};
        state.WorkingBodyA = CopyBodySubgraph(ctxA, *state.BodyA, "_wkA");
        TopologyCopyContext ctxB{*state.TargetModel};
        state.WorkingBodyB = CopyBodySubgraph(ctxB, *state.BodyB, "_wkB");
        if (!state.WorkingBodyA || !state.WorkingBodyB)
        {
            stageResult.Diagnostics = "pipeline Preprocess: failed to clone working bodies";
            return stageResult;
        }
        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Preprocess;
        return stageResult;
    }
};

class IntersectStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Intersect;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Intersect;
        IntersectorRegistry registry;
        const Body& bodyA =
            state.WorkingBodyA != nullptr ? *state.WorkingBodyA : *state.BodyA;
        const Body& bodyB =
            state.WorkingBodyB != nullptr ? *state.WorkingBodyB : *state.BodyB;
        state.FacePairs = CollectFacePairCandidates(bodyA, bodyB);
        state.IntersectionGraph = registry.BuildGraph(bodyA, bodyB, state.Ctx);
        state.Probe =
            registry.ProbeBodyPair(bodyA, bodyB, spatial::BuildQuality::Sah, state.Ctx);
        state.NoVolumeOverlap =
            (state.Op == BooleanOp::Subtract ||
             state.Op == BooleanOp::Union) &&
            !HasPositiveAabbOverlap(
                bodyA, bodyB, std::max(state.Ctx.fuzzy, 1e-9));
        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Intersect;
        return stageResult;
    }
};

class ImprintStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Imprint;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Imprint;

        if (state.NoVolumeOverlap)
        {
            stageResult.Ok = true;
            state.LastCompleted = PipelineStage::Imprint;
            return stageResult;
        }
        const ImprintResult imprint = RunImprint(state);
        if (!imprint.Ok)
        {
            stageResult.Diagnostics = imprint.Diagnostics.empty()
                                          ? "pipeline Imprint: failed"
                                          : imprint.Diagnostics;
            return stageResult;
        }

        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Imprint;
        return stageResult;
    }
};

class ClassifyStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Classify;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Classify;

        if (state.NoVolumeOverlap)
        {
            stageResult.Ok = true;
            state.LastCompleted = PipelineStage::Classify;
            return stageResult;
        }

        const double eps = std::max(state.Ctx.fuzzy, 1e-9);
        const Body& solidA =
            state.WorkingBodyA != nullptr ? *state.WorkingBodyA : *state.BodyA;
        const Body& solidB =
            state.WorkingBodyB != nullptr ? *state.WorkingBodyB : *state.BodyB;

        auto classifyVsB = [&](Face* face) -> FaceRegion
        {
            if (!face)
            {
                return FaceRegion::Unknown;
            }
            return ClassifyFaceAgainstBodyImpl(*face, solidB, solidA, eps,
                                              state.Op, true);
        };

        auto classifyVsA = [&](Face* face) -> FaceRegion
        {
            if (!face)
            {
                return FaceRegion::Unknown;
            }
            return ClassifyFaceAgainstBodyImpl(*face, solidA, solidB, eps,
                                              state.Op, false);
        };

        state.AVsB.clear();
        state.BVsA.clear();
        for (Shell* shell : solidA.Shells)
        {
            if (!shell)
        {
                continue;
            }
            for (Face* face : shell->Faces)
            {
                state.AVsB.push_back(
                    FaceClassification{face, classifyVsB(face)});
            }
        }
        for (Shell* shell : solidB.Shells)
        {
            if (!shell)
        {
                continue;
            }
            for (Face* face : shell->Faces)
            {
                state.BVsA.push_back(FaceClassification{face, classifyVsA(face)});
            }
        }

        const bool haveUnknown =
            std::any_of(state.AVsB.begin(), state.AVsB.end(),
                        [](const FaceClassification& fc)
            {
                            return fc.Region == FaceRegion::Unknown;
                        }) ||
            std::any_of(state.BVsA.begin(), state.BVsA.end(),
                        [](const FaceClassification& fc)
            {
                            return fc.Region == FaceRegion::Unknown;
                        });

        if (haveUnknown)
        {
            stageResult.Diagnostics =
                "pipeline Classify: partial — imprinted face fragments required for "
                "general solids (P1)";
            return stageResult;
        }

        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Classify;
        return stageResult;
    }
};

class SelectStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Select;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Select;

        if (state.NoVolumeOverlap)
        {
            state.Selection = {};
            const Body& bodyA =
                state.WorkingBodyA != nullptr ? *state.WorkingBodyA
                                              : *state.BodyA;
            const Body& bodyB =
                state.WorkingBodyB != nullptr ? *state.WorkingBodyB
                                              : *state.BodyB;
            auto appendFaces =
                [](const Body& body, std::vector<Face*>& dest)
            {
                for (Shell* shell : body.Shells)
                {
                    if (shell == nullptr)
                    {
                        continue;
                    }
                    dest.insert(dest.end(), shell->Faces.begin(),
                                shell->Faces.end());
                }
            };
            if (state.Op == BooleanOp::Union)
            {
                appendFaces(bodyA, state.Selection.FromA);
                appendFaces(bodyB, state.Selection.FromB);
            }
            else
            {
                appendFaces(bodyA, state.Selection.FromA);
            }
        }
        else if (state.AVsB.empty() || state.BVsA.empty())
        {
            stageResult.Diagnostics =
                "pipeline Select: missing classifications (Classify stage required)";
            return stageResult;
        }
        else
        {
            state.Selection =
                SelectCsgFaces(state.Op, state.AVsB, state.BVsA);
        }
        if (state.Selection.FromA.empty() && state.Selection.FromB.empty())
        {
            int inA = 0;
            int onA = 0;
            int outA = 0;
            int inB = 0;
            int onB = 0;
            int outB = 0;
            for (const FaceClassification& fc : state.AVsB)
            {
                switch (fc.Region)
                {
                case FaceRegion::In:
                    ++inA;
                    break;
                case FaceRegion::On:
                    ++onA;
                    break;
                case FaceRegion::Out:
                    ++outA;
                    break;
                default:
                    break;
                }
            }
            for (const FaceClassification& fc : state.BVsA)
            {
                switch (fc.Region)
                {
                case FaceRegion::In:
                    ++inB;
                    break;
                case FaceRegion::On:
                    ++onB;
                    break;
                case FaceRegion::Out:
                    ++outB;
                    break;
                default:
                    break;
                }
            }
            BREP_WARN(
                "pipeline Select: CSG selection empty for {} ('{}' vs '{}'); "
                "A(In/On/Out)={}/{}/{} B(In/On/Out)={}/{}/{} faces={}/{}",
                OpName(state.Op),
                state.BodyA != nullptr ? state.BodyA->Name : "?",
                state.BodyB != nullptr ? state.BodyB->Name : "?",
                inA, onA, outA, inB, onB, outB, state.AVsB.size(),
                state.BVsA.size());
            stageResult.Diagnostics = "pipeline Select: CSG selection empty";
            return stageResult;
        }
        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Select;
        return stageResult;
    }
};

class BuildStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Build;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Build;

        if (state.Selection.FromA.empty() && state.Selection.FromB.empty())
        {
            stageResult.Diagnostics = "pipeline Build: empty selection";
            return stageResult;
        }

        const BooleanBuildResult built =
            BuildBooleanBody(*state.TargetModel, state.Op, state.Selection,
                             std::string("bool_") + state.BodyA->Name + "_" +
                                 state.BodyB->Name);
        if (!built.OutputBody)
        {
            stageResult.Diagnostics = built.Diagnostics.empty()
                                          ? "pipeline Build: assembly failed"
                                          : built.Diagnostics;
            return stageResult;
        }

        state.OutputBody = built.OutputBody;
        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Build;
        return stageResult;
    }
};

}  // namespace

FaceRegion ClassifyFaceAgainstBody(const Face& face, const Body& solid,
                                   const Body& owner, double eps, BooleanOp op,
                                   bool fromA)
{
    return ClassifyFaceAgainstBodyImpl(face, solid, owner, eps, op, fromA);
}

const char* PipelineStageName(PipelineStage stage) noexcept
{
    switch (stage)
{
    case PipelineStage::Preprocess:
        return "Preprocess";
    case PipelineStage::Intersect:
        return "Intersect";
    case PipelineStage::Imprint:
        return "Imprint";
    case PipelineStage::Classify:
        return "Classify";
    case PipelineStage::Select:
        return "Select";
    case PipelineStage::Build:
        return "Build";
    }
    return "Unknown";
}

BooleanPipeline::BooleanPipeline() = default;

void BooleanPipeline::AddStage(std::unique_ptr<IPipelineStage> stage)
{
    if (!stage)
{
        return;
    }
    m_stageOrder.push_back(stage->Id());
    m_stages.push_back(std::move(stage));
}

BooleanResult BooleanPipeline::Evaluate(BooleanOp op, Model& model, const Body& a,
                                        const Body& b, const BooleanContext& ctx)
{
    BooleanResult result;
    result.Mode = BooleanEvalMode::General;

    auto runOrdered = [&](const Body& bodyA,
                          const Body& bodyB) -> BooleanResult
    {
        BooleanResult ordered;
        ordered.Mode = BooleanEvalMode::General;
        PipelineState state;
        state.Op = op;
        state.BodyA = &bodyA;
        state.BodyB = &bodyB;
        state.Ctx = ctx;
        state.TargetModel = &model;

        for (const auto& stage : m_stages)
        {
            AspectEvent event;
            event.Site = "boolean.pipeline";
            event.Subject = PipelineStageName(stage->Id());
            PipelineStageResult stageResult;
            ProcessAspectChain().Invoke(event, [&] {
                stageResult = stage->Run(state);
                event.Failed = !stageResult.Ok;
                event.Detail = stageResult.Diagnostics;
            });
            if (!stageResult.Ok)
            {
                CleanupWorkingBodies(state);
                ordered.Diagnostics =
                    stageResult.Diagnostics.empty()
                        ? std::string("pipeline ") +
                              PipelineStageName(stageResult.Stage) + ": failed"
                        : stageResult.Diagnostics;
                if (state.Probe.CandidatePairs > 0 &&
                    stageResult.Stage != PipelineStage::Intersect)
                {
                    ordered.Diagnostics += "; " + state.Probe.Summary;
                }
                BREP_WARN("{}", ordered.Diagnostics);
                return ordered;
            }
        }

        if (state.OutputBody != nullptr)
        {
            ordered.OutputBody = state.OutputBody;
            ordered.Mode = BooleanEvalMode::General;
            CleanupWorkingBodies(state);
            return ordered;
        }

        CleanupWorkingBodies(state);
        ordered.Diagnostics =
            "pipeline: all stages reported ok but no body was produced "
            "(internal error)";
        BREP_WARN("{}", ordered.Diagnostics);
        return ordered;
    };

    result = runOrdered(a, b);
    if (result.Ok() || !ctx.AllowOperandSwap ||
        (op != BooleanOp::Union && op != BooleanOp::Intersect))
    {
        return result;
    }

    BooleanResult swapped = runOrdered(b, a);
    if (swapped.Ok())
    {
        BREP_INFO("boolean {}: succeeded after swapping operands ('{}' x '{}')",
                  OpName(op), b.Name, a.Name);
        return swapped;
    }
    return result;
}

std::unique_ptr<BooleanPipeline> MakeDefaultBooleanPipeline()
{
    auto pipeline = std::make_unique<BooleanPipeline>();
    pipeline->AddStage(std::make_unique<PreprocessStage>());
    pipeline->AddStage(std::make_unique<IntersectStage>());
    pipeline->AddStage(std::make_unique<ImprintStage>());
    pipeline->AddStage(std::make_unique<ClassifyStage>());
    pipeline->AddStage(std::make_unique<SelectStage>());
    pipeline->AddStage(std::make_unique<BuildStage>());
    return pipeline;
}

}  // namespace brep::boolean
