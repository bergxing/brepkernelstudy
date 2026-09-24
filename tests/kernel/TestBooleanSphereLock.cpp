#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/Mesh.h"
#include "brep/spatial/Aabb.h"
#include "brep/spatial/FaceBvh.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace brep
{
namespace
{

// Baked from untitled.xl sphere pair (Guid lock in TestUntitledSphereExtrudeBoolean).
inline const Point3d kUntitledSmallCenter{
    -2.9938904332874152, 1.2315022069431985, -0.66607390034937941};
inline constexpr double kUntitledSmallRadius = 1.3263314607869463;
inline const Point3d kUntitledLargeCenter{
    -0.41971032900437599, 1.2315022069431985, -0.70031663571947766};
inline constexpr double kUntitledLargeRadius = 2.5348365237903314;

struct BooleanFingerprint
{
    std::size_t Shells{0};
    std::size_t Faces{0};
    std::size_t Planes{0};
    std::size_t Spheres{0};
    std::size_t OuterLoops{0};
    std::size_t InnerLoops{0};
    std::size_t Edges{0};
    std::size_t LineEdges{0};
    std::size_t CircleEdges{0};
    std::size_t Vertices{0};
    std::size_t PartneredCoedges{0};
    std::size_t Degree2Edges{0};
    std::size_t ValidationIssues{0};
    std::size_t ValidationErrors{0};
    bool Closed{false};
    BodyType Type{BodyType::Solid};
    spatial::Aabb VertexBounds{};
    spatial::Aabb FaceBounds{};
};

[[nodiscard]] BooleanFingerprint CollectFingerprint(const Body& body)
{
    BooleanFingerprint fp;
    fp.Type = body.Type;
    fp.Shells = body.Shells.size();

    std::unordered_set<const Edge*> edges;
    std::unordered_set<const Vertex*> vertices;
    for (const Shell* shell : body.Shells)
    {
        if (shell == nullptr)
        {
            continue;
        }
        fp.Closed = fp.Closed || shell->Closed;
        for (const Face* face : shell->Faces)
        {
            if (face == nullptr)
            {
                continue;
            }
            ++fp.Faces;
            if (face->Surface != nullptr)
            {
                if (face->Surface->Kind() == SurfaceKind::Plane)
                {
                    ++fp.Planes;
                }
                if (face->Surface->Kind() == SurfaceKind::Sphere)
                {
                    ++fp.Spheres;
                }
            }
            for (const Loop* loop : face->Loops)
            {
                if (loop == nullptr)
                {
                    continue;
                }
                if (loop->Type == LoopType::Inner)
                {
                    ++fp.InnerLoops;
                }
                else
                {
                    ++fp.OuterLoops;
                }
                loop->ForEachCoedge([&](const CoEdge& coedge)
                {
                    if (coedge.Partner != nullptr)
                    {
                        ++fp.PartneredCoedges;
                    }
                    if (coedge.Edge == nullptr)
                    {
                        return;
                    }
                    edges.insert(coedge.Edge);
                    if (coedge.Edge->V0 != nullptr)
                    {
                        vertices.insert(coedge.Edge->V0);
                    }
                    if (coedge.Edge->V1 != nullptr)
                    {
                        vertices.insert(coedge.Edge->V1);
                    }
                });
            }
        }
    }

    fp.Edges = edges.size();
    fp.Vertices = vertices.size();
    for (const Edge* edge : edges)
    {
        if (edge->Radial.size() == 2U)
        {
            ++fp.Degree2Edges;
        }
        if (edge->Curve != nullptr)
        {
            if (edge->Curve->Kind() == CurveKind::Line)
            {
                ++fp.LineEdges;
            }
            if (edge->Curve->Kind() == CurveKind::Circle)
            {
                ++fp.CircleEdges;
            }
        }
    }
    for (const Vertex* vertex : vertices)
    {
        if (vertex->Point == nullptr)
        {
            continue;
        }
        fp.VertexBounds.Expand(vertex->Position());
    }
    const spatial::FaceBvh bvh = spatial::FaceBvh::Build(body);
    fp.FaceBounds = bvh.RootBounds();

    const ValidationReport report = ValidateBody(body);
    fp.ValidationIssues = report.Issues.size();
    for (const ValidationIssue& issue : report.Issues)
    {
        if (issue.Severity == ValidationIssue::IssueSeverity::Error)
        {
            ++fp.ValidationErrors;
        }
    }
    return fp;
}

[[nodiscard]] std::string FormatFingerprint(const BooleanFingerprint& fp)
{
    std::ostringstream out;
    out << std::setprecision(9) << std::fixed;
    out << "shells=" << fp.Shells << " closed=" << fp.Closed
        << " type=" << static_cast<int>(fp.Type) << " faces=" << fp.Faces
        << " planes=" << fp.Planes << " spheres=" << fp.Spheres
        << " outer=" << fp.OuterLoops << " inner=" << fp.InnerLoops
        << " edges=" << fp.Edges << " lines=" << fp.LineEdges
        << " circles=" << fp.CircleEdges << " verts=" << fp.Vertices
        << " partners=" << fp.PartneredCoedges
        << " deg2=" << fp.Degree2Edges << " issues=" << fp.ValidationIssues
        << " errors=" << fp.ValidationErrors << " vertsAabb=["
        << fp.VertexBounds.Min.x() << ',' << fp.VertexBounds.Min.y() << ','
        << fp.VertexBounds.Min.z() << " | " << fp.VertexBounds.Max.x() << ','
        << fp.VertexBounds.Max.y() << ',' << fp.VertexBounds.Max.z()
        << "] facesAabb=[" << fp.FaceBounds.Min.x() << ','
        << fp.FaceBounds.Min.y() << ',' << fp.FaceBounds.Min.z() << " | "
        << fp.FaceBounds.Max.x() << ',' << fp.FaceBounds.Max.y() << ','
        << fp.FaceBounds.Max.z() << ']';
    return out.str();
}

void ExpectAabbNear(const spatial::Aabb& got, const spatial::Aabb& expected)
{
    constexpr double kBoundTol = 1.0e-6;
    EXPECT_NEAR(got.Min.x(), expected.Min.x(), kBoundTol);
    EXPECT_NEAR(got.Min.y(), expected.Min.y(), kBoundTol);
    EXPECT_NEAR(got.Min.z(), expected.Min.z(), kBoundTol);
    EXPECT_NEAR(got.Max.x(), expected.Max.x(), kBoundTol);
    EXPECT_NEAR(got.Max.y(), expected.Max.y(), kBoundTol);
    EXPECT_NEAR(got.Max.z(), expected.Max.z(), kBoundTol);
}

void ExpectFingerprintEq(const BooleanFingerprint& got,
                         const BooleanFingerprint& expected)
{
    EXPECT_EQ(got.Shells, expected.Shells);
    EXPECT_EQ(got.Closed, expected.Closed);
    EXPECT_EQ(got.Type, expected.Type);
    EXPECT_EQ(got.Faces, expected.Faces);
    EXPECT_EQ(got.Planes, expected.Planes);
    EXPECT_EQ(got.Spheres, expected.Spheres);
    EXPECT_EQ(got.OuterLoops, expected.OuterLoops);
    EXPECT_EQ(got.InnerLoops, expected.InnerLoops);
    EXPECT_EQ(got.Edges, expected.Edges);
    EXPECT_EQ(got.LineEdges, expected.LineEdges);
    EXPECT_EQ(got.CircleEdges, expected.CircleEdges);
    EXPECT_EQ(got.Vertices, expected.Vertices);
    EXPECT_EQ(got.PartneredCoedges, expected.PartneredCoedges);
    EXPECT_EQ(got.Degree2Edges, expected.Degree2Edges);
    EXPECT_EQ(got.ValidationIssues, expected.ValidationIssues);
    EXPECT_EQ(got.ValidationErrors, expected.ValidationErrors);
    ExpectAabbNear(got.VertexBounds, expected.VertexBounds);
    ExpectAabbNear(got.FaceBounds, expected.FaceBounds);
}

[[nodiscard]] spatial::Aabb MakeAabb(double minX, double minY, double minZ,
                                     double maxX, double maxY, double maxZ)
{
    spatial::Aabb box;
    box.Min = Point3d{minX, minY, minZ};
    box.Max = Point3d{maxX, maxY, maxZ};
    return box;
}

[[nodiscard]] std::pair<Body*, Body*> MakeUntitledSpheres(Model& model)
{
    Body* small = MakeSphere(
        model, SphereSpec{.Center = kUntitledSmallCenter,
                          .Radius = kUntitledSmallRadius,
                          .Name = "sphereSmall"});
    Body* large = MakeSphere(
        model, SphereSpec{.Center = kUntitledLargeCenter,
                          .Radius = kUntitledLargeRadius,
                          .Name = "sphereLarge"});
    return {small, large};
}

void ExpectBooleanOrdered(boolean::BooleanOp op, Model& model, const Body& a,
                          const Body& b, const BooleanFingerprint& expected)
{
    boolean::BooleanContext ctx;
    ctx.AllowOperandSwap = false;
    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto result = eval->Evaluate(op, model, a, b, ctx);
    ASSERT_TRUE(result.Ok()) << result.Diagnostics;
    ASSERT_NE(result.OutputBody, nullptr);
    const BooleanFingerprint got = CollectFingerprint(*result.OutputBody);
    SCOPED_TRACE(FormatFingerprint(got));
    EXPECT_EQ(got.ValidationErrors, 0U);
    ExpectFingerprintEq(got, expected);
}

void ExpectBooleanBothOrders(boolean::BooleanOp op, Model& model, Body& a,
                             Body& b, const BooleanFingerprint& expected)
{
    ExpectBooleanOrdered(op, model, a, b, expected);
    ExpectBooleanOrdered(op, model, b, a, expected);
}

[[nodiscard]] const CircleCurve* FirstCircleOnFace(const Face& face)
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
                coedge->Edge->Curve->Kind() == CurveKind::Circle)
            {
                return static_cast<const CircleCurve*>(coedge->Edge->Curve);
            }
            coedge = coedge->Next;
        } while (coedge != nullptr && coedge != loop->First);
    }
    return nullptr;
}

[[nodiscard]] Point3d SphereCapPole(const SphereSurface& sphere,
                                    const CircleCurve& circle)
{
    Vector3d axis = circle.Center() - sphere.Center();
    if (axis.norm() <= 1e-9)
    {
        axis = circle.Normal();
    }
    if (axis.norm() <= 1e-9)
    {
        return sphere.Eval(0.25, 0.25);
    }
    return sphere.Center() + axis.normalized() * sphere.Radius();
}

void ExpectLensIsTwoSphericalCaps(const Body& body)
{
    std::vector<const Face*> sphereFaces;
    for (const Shell* shell : body.Shells)
    {
        if (shell == nullptr)
        {
            continue;
        }
        for (const Face* face : shell->Faces)
        {
            if (face != nullptr && face->Surface != nullptr &&
                face->Surface->Kind() == SurfaceKind::Sphere)
            {
                sphereFaces.push_back(face);
            }
        }
    }
    ASSERT_EQ(sphereFaces.size(), 2U) << "intersect must be two spherical caps";

    std::vector<Point3d> centers;
    for (const Face* face : sphereFaces)
    {
        const auto& sphere =
            static_cast<const SphereSurface&>(*face->Surface);
        EXPECT_TRUE(face->Name.find("_circle_imprint_cap") != std::string::npos)
            << face->Name << ": intersect face must be an imprint cap";
        const CircleCurve* circle = FirstCircleOnFace(*face);
        ASSERT_NE(circle, nullptr) << face->Name;

        bool knownCenter = false;
        if ((sphere.Center() - kUntitledSmallCenter).norm() <= 1e-6)
        {
            EXPECT_NEAR(sphere.Radius(), kUntitledSmallRadius, 1e-6)
                << face->Name;
            knownCenter = true;
        }
        if ((sphere.Center() - kUntitledLargeCenter).norm() <= 1e-6)
        {
            EXPECT_NEAR(sphere.Radius(), kUntitledLargeRadius, 1e-6)
                << face->Name;
            knownCenter = true;
        }
        EXPECT_TRUE(knownCenter) << face->Name << ": unexpected sphere center";

        const Point3d pole = SphereCapPole(sphere, *circle);
        const Point3d onFace = face->Surface->Eval(0.25, 0.25);
        EXPECT_NEAR((onFace - sphere.Center()).norm(), sphere.Radius(), 1e-5)
            << face->Name << ": UV sample must lie on the sphere";
        EXPECT_NEAR((pole - sphere.Center()).norm(), sphere.Radius(), 1e-5)
            << face->Name << ": cap pole must lie on the sphere";

        bool seen = false;
        for (const Point3d& existing : centers)
        {
            if ((existing - sphere.Center()).norm() <= 1e-6)
            {
                seen = true;
                break;
            }
        }
        EXPECT_FALSE(seen) << "duplicate sphere center in intersect result";
        centers.push_back(sphere.Center());
    }
}

void ExpectMeshNormalsMatchSphere(const Body& body, double minRadialDot)
{
    const TriangleMesh mesh = TessellateBody(body);
    ASSERT_GE(mesh.Indices.size(), 9U);
    for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3U)
    {
        const Point3d& a = mesh.Vertices[mesh.Indices[i]].Position;
        const Point3d& b = mesh.Vertices[mesh.Indices[i + 1U]].Position;
        const Point3d& c = mesh.Vertices[mesh.Indices[i + 2U]].Position;
        const Point3d centroid{
            (a.x() + b.x() + c.x()) / 3.0,
            (a.y() + b.y() + c.y()) / 3.0,
            (a.z() + b.z() + c.z()) / 3.0,
        };
        double bestDot = -1.0;
        for (const Shell* shell : body.Shells)
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
                const auto& sphere =
                    static_cast<const SphereSurface&>(*face->Surface);
                const Vector3d radial = centroid - sphere.Center();
                if (radial.norm() <= 1e-9)
                {
                    continue;
                }
                const Vector3d normal = mesh.Vertices[mesh.Indices[i]].Normal;
                bestDot = std::max(bestDot, radial.normalized().dot(normal));
            }
        }
        EXPECT_GE(bestDot, minRadialDot)
            << "triangle normal must align with sphere radial (not conical)";
    }
}

}  // namespace

TEST(BooleanSphereLock, UntitledIntersectBothOrdersNoSwap)
{
    Model model;
    auto [small, large] = MakeUntitledSpheres(model);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(large, nullptr);

    BooleanFingerprint intersectFp;
    intersectFp.Shells = 1;
    intersectFp.Closed = true;
    intersectFp.Type = BodyType::Solid;
    intersectFp.Faces = 2;
    intersectFp.Spheres = 2;
    intersectFp.OuterLoops = 2;
    intersectFp.Edges = 2;
    intersectFp.CircleEdges = 2;
    intersectFp.Vertices = 2;
    intersectFp.PartneredCoedges = 4;
    intersectFp.Degree2Edges = 2;
    intersectFp.ValidationIssues = 2;
    intersectFp.VertexBounds = MakeAabb(-2.629893386, 1.231502207, -1.941480225,
                                      -2.596096295, 1.231502207, 0.599198785);
    intersectFp.FaceBounds = MakeAabb(-4.320221894, -1.303334317, -3.235153160,
                                      2.115126195, 3.766338731, 1.834519888);

    boolean::BooleanContext ctx;
    ctx.AllowOperandSwap = false;
    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto result =
        eval->Evaluate(boolean::BooleanOp::Intersect, model, *small, *large, ctx);
    ASSERT_TRUE(result.Ok()) << result.Diagnostics;
    ASSERT_NE(result.OutputBody, nullptr);
    ExpectLensIsTwoSphericalCaps(*result.OutputBody);
    ExpectMeshNormalsMatchSphere(*result.OutputBody, 0.92);

    ExpectBooleanBothOrders(boolean::BooleanOp::Intersect, model, *small, *large,
                            intersectFp);
}

TEST(BooleanSphereLock, UntitledUnionBothOrdersNoSwap)
{
    Model model;
    auto [small, large] = MakeUntitledSpheres(model);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(large, nullptr);

    BooleanFingerprint unionFp;
    unionFp.Shells = 1;
    unionFp.Closed = true;
    unionFp.Type = BodyType::Solid;
    unionFp.Faces = 2;
    unionFp.Spheres = 2;
    unionFp.OuterLoops = 2;
    unionFp.InnerLoops = 2;
    unionFp.Edges = 4;
    unionFp.CircleEdges = 4;
    unionFp.Vertices = 6;
    unionFp.PartneredCoedges = 8;
    unionFp.Degree2Edges = 4;
    unionFp.ValidationIssues = 4;
    unionFp.VertexBounds = MakeAabb(-2.993890433, -1.303334317, -1.941480225,
                                    -0.419710329, 3.766338731, 0.599198785);
    unionFp.FaceBounds = MakeAabb(-4.320221894, -1.303334317, -3.235153160,
                                  2.115126195, 3.766338731, 1.834519888);

    ExpectBooleanBothOrders(boolean::BooleanOp::Union, model, *small, *large,
                            unionFp);
}

TEST(BooleanSphereLock, UntitledUnionMeshMeetsAtCircle)
{
    Model model;
    auto [small, large] = MakeUntitledSpheres(model);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(large, nullptr);

    boolean::BooleanContext ctx;
    ctx.AllowOperandSwap = false;
    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto result = eval->Evaluate(boolean::BooleanOp::Union, model, *small,
                                       *large, ctx);
    ASSERT_TRUE(result.Ok()) << result.Diagnostics;
    ASSERT_NE(result.OutputBody, nullptr);

    const TriangleMesh mesh = TessellateBody(*result.OutputBody);
    ASSERT_GT(mesh.Indices.size(), 0U);

    const Vector3d delta = kUntitledSmallCenter - kUntitledLargeCenter;
    const double distance = delta.norm();
    ASSERT_GT(distance, 1e-9);
    const Vector3d axis = delta / distance;
    const double planeFromLarge =
        (distance * distance + kUntitledLargeRadius * kUntitledLargeRadius -
         kUntitledSmallRadius * kUntitledSmallRadius) /
        (2.0 * distance);
    const Point3d circleCenter =
        kUntitledLargeCenter + axis * planeFromLarge;
    const double circleRadiusSq =
        kUntitledLargeRadius * kUntitledLargeRadius - planeFromLarge * planeFromLarge;
    ASSERT_GT(circleRadiusSq, 0.0);
    const double circleRadius = std::sqrt(circleRadiusSq);

    struct EdgeHash
    {
        std::size_t operator()(
            const std::pair<std::uint32_t, std::uint32_t>& edge) const
        {
            return (static_cast<std::uint64_t>(edge.first) << 32U) | edge.second;
        }
    };
    std::unordered_map<std::pair<std::uint32_t, std::uint32_t>, int, EdgeHash>
        uses;
    for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3U)
    {
        const std::uint32_t tri[3] = {mesh.Indices[i], mesh.Indices[i + 1U],
                                      mesh.Indices[i + 2U]};
        for (int e = 0; e < 3; ++e)
        {
            std::uint32_t a = tri[e];
            std::uint32_t b = tri[(e + 1) % 3];
            if (a > b)
            {
                std::swap(a, b);
            }
            ++uses[{a, b}];
        }
    }

    int boundaryVertices = 0;
    double maxCircleDistance = 0.0;
    for (const auto& [edge, count] : uses)
    {
        if (count != 1)
        {
            continue;
        }
        for (const std::uint32_t index : {edge.first, edge.second})
        {
            const Point3d& position = mesh.Vertices[index].Position;
            const double plane = (position - circleCenter).dot(axis);
            const Vector3d inPlane = (position - circleCenter) - axis * plane;
            const double radial = inPlane.norm() - circleRadius;
            const double circleDistance = std::sqrt(plane * plane + radial * radial);
            maxCircleDistance = std::max(maxCircleDistance, circleDistance);
            ++boundaryVertices;
        }
    }
    EXPECT_GT(boundaryVertices, 0) << "union rim must exist along the intersection";
    EXPECT_LT(maxCircleDistance, 0.05)
        << "union rim must lie on the intersection circle, not a jagged grid hole";
}

TEST(BooleanSphereLock, UntitledLargeMinusSmallNoSwap)
{
    Model model;
    auto [small, large] = MakeUntitledSpheres(model);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(large, nullptr);

    BooleanFingerprint cutFp;
    cutFp.Shells = 1;
    cutFp.Closed = true;
    cutFp.Type = BodyType::Solid;
    cutFp.Faces = 2;
    cutFp.Spheres = 2;
    cutFp.OuterLoops = 2;
    cutFp.InnerLoops = 1;
    cutFp.Edges = 3;
    cutFp.CircleEdges = 3;
    cutFp.Vertices = 4;
    cutFp.PartneredCoedges = 6;
    cutFp.Degree2Edges = 3;
    cutFp.ValidationIssues = 3;
    cutFp.VertexBounds = MakeAabb(-2.629893386, -1.303334317, -1.941480225,
                                  -0.419710329, 3.766338731, 0.599198785);
    cutFp.FaceBounds = MakeAabb(-4.320221894, -1.303334317, -3.235153160,
                                2.115126195, 3.766338731, 1.834519888);

    ExpectBooleanOrdered(boolean::BooleanOp::Subtract, model, *large, *small,
                         cutFp);
}

// sphere_copy from untitled.xl. Its center sits inside the large sphere and
// the smaller cap is the part that sticks out, so a full-sphere mesh or the
// outside cap kept as the intersection both fail these counts.
void CountMeshAgainstSphere(const TriangleMesh& mesh, const Point3d& center,
                            double radius, double margin, int& deepInside,
                            int& clearOutside)
{
    deepInside = 0;
    clearOutside = 0;
    for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3U)
    {
        const Point3d& a = mesh.Vertices[mesh.Indices[i]].Position;
        const Point3d& b = mesh.Vertices[mesh.Indices[i + 1U]].Position;
        const Point3d& c = mesh.Vertices[mesh.Indices[i + 2U]].Position;
        const Point3d centroid{(a.x() + b.x() + c.x()) / 3.0,
                               (a.y() + b.y() + c.y()) / 3.0,
                               (a.z() + b.z() + c.z()) / 3.0};
        const double distance = (centroid - center).norm();
        if (distance < radius - margin)
        {
            ++deepInside;
        }
        else if (distance > radius + margin)
        {
            ++clearOutside;
        }
    }
}

TEST(BooleanSphereLock, CopiedOverlapMeshFollowsSolid)
{
    const Point3d copyCenter{-2.1356009845631512, 1.2315022069431976,
                             0.47503994266110494};
    constexpr double copyRadius = 1.3263314607869463;
    constexpr double margin = 0.2;

    Model model;
    Body* copy = MakeSphere(model, SphereSpec{.Center = copyCenter,
                                               .Radius = copyRadius,
                                               .Name = "sphereCopy"});
    Body* large = MakeSphere(model, SphereSpec{.Center = kUntitledLargeCenter,
                                                .Radius = kUntitledLargeRadius,
                                                .Name = "sphereLarge"});
    ASSERT_NE(copy, nullptr);
    ASSERT_NE(large, nullptr);

    boolean::BooleanContext ctx;
    ctx.AllowOperandSwap = false;
    auto eval = boolean::MakeDefaultBooleanEvaluator();

    const auto expectSolid = [](const boolean::BooleanResult& result)
    {
        ASSERT_TRUE(result.Ok()) << result.Diagnostics;
        ASSERT_NE(result.OutputBody, nullptr);
        EXPECT_EQ(CollectFingerprint(*result.OutputBody).ValidationErrors, 0U);
    };

    const auto united =
        eval->Evaluate(boolean::BooleanOp::Union, model, *copy, *large, ctx);
    expectSolid(united);
    {
        const TriangleMesh mesh = TessellateBody(*united.OutputBody);
        int insideCopy = 0;
        int outsideLarge = 0;
        int insideLarge = 0;
        int unused = 0;
        CountMeshAgainstSphere(mesh, copyCenter, copyRadius, margin, insideCopy,
                               unused);
        CountMeshAgainstSphere(mesh, kUntitledLargeCenter, kUntitledLargeRadius,
                               margin, insideLarge, outsideLarge);
        EXPECT_EQ(insideCopy, 0) << "union must punch the cap out of the large sphere";
        EXPECT_EQ(insideLarge, 0) << "union must not keep the small sphere's interior";
        EXPECT_GT(outsideLarge, 0) << "union must draw the protruding small cap";
    }

    const auto common = eval->Evaluate(boolean::BooleanOp::Intersect, model,
                                       *copy, *large, ctx);
    expectSolid(common);
    {
        const TriangleMesh mesh = TessellateBody(*common.OutputBody);
        int insideCopy = 0;
        int outsideCopy = 0;
        int insideLarge = 0;
        int outsideLarge = 0;
        CountMeshAgainstSphere(mesh, copyCenter, copyRadius, margin, insideCopy,
                               outsideCopy);
        CountMeshAgainstSphere(mesh, kUntitledLargeCenter, kUntitledLargeRadius,
                               margin, insideLarge, outsideLarge);
        EXPECT_EQ(outsideCopy, 0) << "intersect must lie inside the copy";
        EXPECT_EQ(outsideLarge, 0) << "intersect must lie inside the large sphere";
        EXPECT_GT(insideLarge, 0) << "intersect must include the overlap surface";
        EXPECT_GT(mesh.Indices.size(), 0U);
    }

    const auto cut = eval->Evaluate(boolean::BooleanOp::Subtract, model, *large,
                                    *copy, ctx);
    expectSolid(cut);
    {
        const TriangleMesh mesh = TessellateBody(*cut.OutputBody);
        int insideCopy = 0;
        int outsideLarge = 0;
        int unused = 0;
        CountMeshAgainstSphere(mesh, copyCenter, copyRadius, margin, insideCopy,
                               unused);
        CountMeshAgainstSphere(mesh, kUntitledLargeCenter, kUntitledLargeRadius,
                               margin, unused, outsideLarge);
        EXPECT_EQ(insideCopy, 0)
            << "large-minus-copy must punch the cap out of the large sphere";
        EXPECT_EQ(outsideLarge, 0)
            << "large-minus-copy must drop the small sphere's exterior";
    }
}

}  // namespace brep
