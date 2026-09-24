#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/ops/Profile.h"
#include "brep/spatial/Aabb.h"
#include "brep/spatial/FaceBvh.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace brep
{
namespace
{

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

[[nodiscard]] std::string ValidationErrors(const Body& body)
{
    std::ostringstream out;
    for (const ValidationIssue& issue : ValidateBody(body).Issues)
    {
        if (issue.Severity == ValidationIssue::IssueSeverity::Error)
        {
            out << issue.Where << ": " << issue.Message << '\n';
        }
    }
    return out.str();
}

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
                loop->ForEachCoedge([&](const CoEdge& coedge) {
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
    EXPECT_EQ(got.ValidationErrors, 0U) << ValidationErrors(*result.OutputBody);
    ExpectFingerprintEq(got, expected);
}

[[nodiscard]] spatial::Aabb MakeAabb(double minX, double minY, double minZ,
                                     double maxX, double maxY, double maxZ)
{
    spatial::Aabb box;
    box.Min = Point3d{minX, minY, minZ};
    box.Max = Point3d{maxX, maxY, maxZ};
    return box;
}

void ExpectBooleanBothOrders(boolean::BooleanOp op, Model& model, Body& a,
                             Body& b, const BooleanFingerprint& expected)
{
    ExpectBooleanOrdered(op, model, a, b, expected);
    ExpectBooleanOrdered(op, model, b, a, expected);
}

[[nodiscard]] Body* MakeRectPad(Model& model)
{
    ops::ExtrudeSpec spec;
    spec.Name = "pad";
    spec.Plane = Plane::XzYUp();
    spec.Distance = 1.0;
    spec.Profile.Outer = {
        Point2d{0, 0},
        Point2d{2, 0},
        Point2d{2, 2},
        Point2d{0, 2},
    };
    return ops::Extrude(model, spec);
}

[[nodiscard]] Body* MakeHexPad(Model& model)
{
    ops::ExtrudeSpec spec;
    spec.Name = "Pad";
    spec.Plane = Plane::XzYUp();
    spec.Distance = 1.2315022069431985;
    spec.Profile.Outer.reserve(6U);
    constexpr double kHexRadius = 2.0;
    for (int i = 0; i < 6; ++i)
    {
        const double angle =
            static_cast<double>(i) * (std::numbers::pi / 3.0);
        spec.Profile.Outer.push_back(Point2d{kHexRadius * std::cos(angle),
                                            kHexRadius * std::sin(angle)});
    }
    return ops::Extrude(model, spec);
}

TEST(ExtrudePadBoolean, BoxUnionOverlappingExtrudePadValidates)
{
  auto doc = Document::Create("extrude_pad_bool");
  Part& part = doc->AddPart("Main");
  Body* box = part.AddBox(
      BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 2}, .Name = "box"});
  ASSERT_NE(box, nullptr);

  const std::vector<Point2d> profile = {
      Point2d{1, 0},
      Point2d{3, 0},
      Point2d{3, 2},
      Point2d{1, 2},
  };
  const ExtrudePadResult pad =
      part.AddExtrudePad(profile, 1.0, false, "pad");
  ASSERT_NE(pad.Body, nullptr);

  const auto* boxFeat = part.Features().FindByBody(box->Guid);
  const auto* padFeat = part.Features().FindByBody(pad.Body->Guid);
  ASSERT_NE(boxFeat, nullptr);
  ASSERT_NE(padFeat, nullptr);

  Body* fused = part.AddBoolean(boolean::BooleanOp::Union, boxFeat->Id(),
                                padFeat->Id(), "Fuse");
  ASSERT_NE(fused, nullptr) << part.LastRegenError();
  EXPECT_TRUE(ValidateBody(*fused).Ok());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
}

TEST(ExtrudePadBoolean, DisjointBoxUnionExtrudePadValidates)
{
  auto doc = Document::Create("extrude_pad_bool_disjoint");
  Part& part = doc->AddPart("Main");
  Body* box = part.AddBox(
      BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
  ASSERT_NE(box, nullptr);

  const std::vector<Point2d> profile = {
      Point2d{3, 0},
      Point2d{4, 0},
      Point2d{4, 1},
      Point2d{3, 1},
  };
  const ExtrudePadResult pad =
      part.AddExtrudePad(profile, 1.0, false, "pad");
  ASSERT_NE(pad.Body, nullptr);

  const auto* boxFeat = part.Features().FindByBody(box->Guid);
  const auto* padFeat = part.Features().FindByBody(pad.Body->Guid);
  ASSERT_NE(boxFeat, nullptr);
  ASSERT_NE(padFeat, nullptr);

  Body* fused = part.AddBoolean(boolean::BooleanOp::Union, boxFeat->Id(),
                                padFeat->Id(), "Fuse");
  ASSERT_NE(fused, nullptr) << part.LastRegenError();
  EXPECT_TRUE(ValidateBody(*fused).Ok());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
}

TEST(ExtrudePadBoolean, PadUnionSphereValidates)
{
  auto doc = Document::Create("pad_sphere_union");
  Part& part = doc->AddPart("Main");

  const std::vector<Point2d> profile = {
      Point2d{0, 0},
      Point2d{2, 0},
      Point2d{2, 2},
      Point2d{0, 2},
  };
  const ExtrudePadResult pad =
      part.AddExtrudePad(profile, 1.0, false, "pad");
  ASSERT_NE(pad.Body, nullptr);

  Body* sphere = part.AddSphere(
      SphereSpec{.Center = {1.0, 0.5, 1.0}, .Radius = 0.8, .Name = "sphere"});
  ASSERT_NE(sphere, nullptr);

  const auto* padFeat = part.Features().FindByBody(pad.Body->Guid);
  const auto* sphereFeat = part.Features().FindByBody(sphere->Guid);
  ASSERT_NE(padFeat, nullptr);
  ASSERT_NE(sphereFeat, nullptr);

  Body* fused = part.AddBoolean(boolean::BooleanOp::Union, padFeat->Id(),
                                sphereFeat->Id(), "Fuse");
  ASSERT_NE(fused, nullptr) << part.LastRegenError();
  EXPECT_TRUE(ValidateBody(*fused).Ok());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
}

TEST(ExtrudePadBoolean, SphereUnionPadValidates)
{
  auto doc = Document::Create("sphere_pad_union");
  Part& part = doc->AddPart("Main");

  const std::vector<Point2d> profile = {
      Point2d{0, 0},
      Point2d{2, 0},
      Point2d{2, 2},
      Point2d{0, 2},
  };
  const ExtrudePadResult pad =
      part.AddExtrudePad(profile, 1.0, false, "pad");
  ASSERT_NE(pad.Body, nullptr);

  Body* sphere = part.AddSphere(
      SphereSpec{.Center = {1.0, 0.5, 1.0}, .Radius = 0.8, .Name = "sphere"});
  ASSERT_NE(sphere, nullptr);

  const auto* padFeat = part.Features().FindByBody(pad.Body->Guid);
  const auto* sphereFeat = part.Features().FindByBody(sphere->Guid);
  ASSERT_NE(padFeat, nullptr);
  ASSERT_NE(sphereFeat, nullptr);

  Body* fused = part.AddBoolean(boolean::BooleanOp::Union, sphereFeat->Id(),
                                padFeat->Id(), "Fuse");
  ASSERT_NE(fused, nullptr) << part.LastRegenError();
  EXPECT_TRUE(ValidateBody(*fused).Ok());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
}

TEST(ExtrudePadBoolean, PadIntersectSphereValidates)
{
  auto doc = Document::Create("pad_sphere_intersect");
  Part& part = doc->AddPart("Main");

  const std::vector<Point2d> profile = {
      Point2d{0, 0},
      Point2d{2, 0},
      Point2d{2, 2},
      Point2d{0, 2},
  };
  const ExtrudePadResult pad =
      part.AddExtrudePad(profile, 1.0, false, "Pad");
  ASSERT_NE(pad.Body, nullptr);

  Body* sphere = part.AddSphere(
      SphereSpec{.Center = {1.0, 0.5, 1.0}, .Radius = 0.8, .Name = "sphere"});
  ASSERT_NE(sphere, nullptr);

  const auto* padFeat = part.Features().FindByBody(pad.Body->Guid);
  const auto* sphereFeat = part.Features().FindByBody(sphere->Guid);
  ASSERT_NE(padFeat, nullptr);
  ASSERT_NE(sphereFeat, nullptr);

  Body* common = part.AddBoolean(boolean::BooleanOp::Intersect, sphereFeat->Id(),
                                 padFeat->Id(), "Common");
  ASSERT_NE(common, nullptr) << part.LastRegenError();
  EXPECT_TRUE(ValidateBody(*common).Ok());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
}

TEST(ExtrudePadBoolean, HexPadIntersectSphereValidates)
{
  auto doc = Document::Create("hex_pad_sphere_intersect");
  Part& part = doc->AddPart("Main");

  std::vector<Point2d> profile;
  profile.reserve(6U);
  constexpr double kHexRadius = 2.0;
  for (int i = 0; i < 6; ++i)
  {
    const double angle =
        static_cast<double>(i) * (std::numbers::pi / 3.0);
    profile.push_back(Point2d{kHexRadius * std::cos(angle),
                              kHexRadius * std::sin(angle)});
  }
  const ExtrudePadResult pad =
      part.AddExtrudePad(profile, 1.2315022069431985, false, "Pad");
  ASSERT_NE(pad.Body, nullptr);

  Body* sphere = part.AddSphere(SphereSpec{
      .Center = {-1.76672, 1.2315, -0.954155},
      .Radius = 2.53484,
      .Name = "sphere"});
  ASSERT_NE(sphere, nullptr);

  const auto* padFeat = part.Features().FindByBody(pad.Body->Guid);
  const auto* sphereFeat = part.Features().FindByBody(sphere->Guid);
  ASSERT_NE(padFeat, nullptr);
  ASSERT_NE(sphereFeat, nullptr);

  Body* common = part.AddBoolean(boolean::BooleanOp::Intersect, sphereFeat->Id(),
                                 padFeat->Id(), "Common");
  ASSERT_NE(common, nullptr) << part.LastRegenError();
  EXPECT_TRUE(ValidateBody(*common).Ok());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
}

TEST(BooleanLock, RectPadSphereBothOrdersNoSwap)
{
    Model model;
    Body* pad = MakeRectPad(model);
    Body* sphere = MakeSphere(
        model, SphereSpec{.Center = {1.0, 0.5, 1.0}, .Radius = 0.8,
                          .Name = "sphere"});
    ASSERT_NE(pad, nullptr);
    ASSERT_NE(sphere, nullptr);

    BooleanFingerprint unionFp;
    unionFp.Shells = 1;
    unionFp.Closed = true;
    unionFp.Type = BodyType::Solid;
    unionFp.Faces = 9;
    unionFp.Planes = 6;
    unionFp.Spheres = 3;
    unionFp.OuterLoops = 9;
    unionFp.InnerLoops = 2;
    unionFp.Edges = 17;
    unionFp.LineEdges = 12;
    unionFp.CircleEdges = 5;
    unionFp.Vertices = 14;
    unionFp.PartneredCoedges = 34;
    unionFp.Degree2Edges = 17;
    unionFp.ValidationIssues = 5;
    unionFp.VertexBounds = MakeAabb(0.0, -0.3, 0.0, 2.0, 1.3, 2.0);
    unionFp.FaceBounds = MakeAabb(0.0, -0.3, 0.0, 2.0, 1.3, 2.0);

    BooleanFingerprint intersectFp;
    intersectFp.Shells = 1;
    intersectFp.Closed = true;
    intersectFp.Type = BodyType::Solid;
    intersectFp.Faces = 3;
    intersectFp.Planes = 2;
    intersectFp.Spheres = 1;
    intersectFp.OuterLoops = 3;
    intersectFp.InnerLoops = 2;
    intersectFp.Edges = 5;
    intersectFp.CircleEdges = 5;
    intersectFp.Vertices = 6;
    intersectFp.PartneredCoedges = 10;
    intersectFp.Degree2Edges = 5;
    intersectFp.ValidationIssues = 5;
    intersectFp.VertexBounds =
        MakeAabb(1.0, -0.3, 0.375500200, 1.0, 1.3, 1.624499800);
    intersectFp.FaceBounds = MakeAabb(0.2, -0.3, 0.2, 1.8, 1.3, 1.8);

    BooleanFingerprint padMinusSphere;
    padMinusSphere.Shells = 1;
    padMinusSphere.Closed = true;
    padMinusSphere.Type = BodyType::Solid;
    padMinusSphere.Faces = 7;
    padMinusSphere.Planes = 6;
    padMinusSphere.Spheres = 1;
    padMinusSphere.OuterLoops = 7;
    padMinusSphere.InnerLoops = 4;
    padMinusSphere.Edges = 17;
    padMinusSphere.LineEdges = 12;
    padMinusSphere.CircleEdges = 5;
    padMinusSphere.Vertices = 14;
    padMinusSphere.PartneredCoedges = 34;
    padMinusSphere.Degree2Edges = 17;
    padMinusSphere.ValidationIssues = 5;
    padMinusSphere.VertexBounds = MakeAabb(0.0, -0.3, 0.0, 2.0, 1.3, 2.0);
    padMinusSphere.FaceBounds = MakeAabb(0.0, -0.3, 0.0, 2.0, 1.3, 2.0);

    BooleanFingerprint sphereMinusPad;
    sphereMinusPad.Shells = 1;
    sphereMinusPad.Closed = true;
    sphereMinusPad.Type = BodyType::Solid;
    sphereMinusPad.Faces = 4;
    sphereMinusPad.Planes = 2;
    sphereMinusPad.Spheres = 2;
    sphereMinusPad.OuterLoops = 4;
    sphereMinusPad.Edges = 4;
    sphereMinusPad.CircleEdges = 4;
    sphereMinusPad.Vertices = 4;
    sphereMinusPad.PartneredCoedges = 8;
    sphereMinusPad.Degree2Edges = 4;
    sphereMinusPad.ValidationIssues = 4;
    sphereMinusPad.VertexBounds =
        MakeAabb(1.0, 0.0, 0.375500200, 1.0, 1.0, 1.624499800);
    sphereMinusPad.FaceBounds = MakeAabb(0.2, -0.3, 0.2, 1.8, 1.3, 1.8);

    ExpectBooleanBothOrders(boolean::BooleanOp::Union, model, *pad, *sphere,
                            unionFp);
    ExpectBooleanBothOrders(boolean::BooleanOp::Intersect, model, *pad, *sphere,
                            intersectFp);
    ExpectBooleanOrdered(boolean::BooleanOp::Subtract, model, *pad, *sphere,
                         padMinusSphere);
    ExpectBooleanOrdered(boolean::BooleanOp::Subtract, model, *sphere, *pad,
                         sphereMinusPad);
}

TEST(BooleanLock, HexPadSphereBothOrdersNoSwap)
{
    Model model;
    Body* pad = MakeHexPad(model);
    Body* sphere = MakeSphere(
        model, SphereSpec{.Center = {-1.76672, 1.2315, -0.954155},
                          .Radius = 2.53484,
                          .Name = "sphere"});
    ASSERT_NE(pad, nullptr);
    ASSERT_NE(sphere, nullptr);

    BooleanFingerprint intersectFp;
    intersectFp.Shells = 1;
    intersectFp.Closed = true;
    intersectFp.Type = BodyType::Solid;
    intersectFp.Faces = 6;
    intersectFp.Planes = 5;
    intersectFp.Spheres = 1;
    intersectFp.OuterLoops = 6;
    intersectFp.Edges = 12;
    intersectFp.LineEdges = 8;
    intersectFp.CircleEdges = 4;
    intersectFp.Vertices = 8;
    intersectFp.PartneredCoedges = 24;
    intersectFp.Degree2Edges = 12;
    intersectFp.VertexBounds = MakeAabb(-2.0, 0.0, -1.732050808, 0.645808122,
                                        1.231502207, 1.500384845);
    intersectFp.FaceBounds = MakeAabb(-4.30156, -1.30334, -3.488995, 0.76812,
                                      3.76634, 1.580685);

    BooleanFingerprint unionFp;
    unionFp.Shells = 1;
    unionFp.Closed = true;
    unionFp.Type = BodyType::Solid;
    unionFp.Faces = 8;
    unionFp.Planes = 7;
    unionFp.Spheres = 1;
    unionFp.OuterLoops = 8;
    unionFp.InnerLoops = 1;
    unionFp.Edges = 19;
    unionFp.LineEdges = 14;
    unionFp.CircleEdges = 5;
    unionFp.Vertices = 14;
    unionFp.PartneredCoedges = 38;
    unionFp.Degree2Edges = 19;
    unionFp.ValidationIssues = 1;
    unionFp.VertexBounds = MakeAabb(-1.76672, -1.30334, -1.732050808, 2.0,
                                    3.76634, 1.732050808);
    unionFp.FaceBounds = MakeAabb(-4.30156, -1.30334, -3.488995, 2.0, 3.76634,
                                  1.732050808);

    BooleanFingerprint padMinusSphere;
    padMinusSphere.Shells = 1;
    padMinusSphere.Closed = true;
    padMinusSphere.Type = BodyType::Solid;
    padMinusSphere.Faces = 8;
    padMinusSphere.Planes = 7;
    padMinusSphere.Spheres = 1;
    padMinusSphere.OuterLoops = 8;
    padMinusSphere.Edges = 18;
    padMinusSphere.LineEdges = 14;
    padMinusSphere.CircleEdges = 4;
    padMinusSphere.Vertices = 12;
    padMinusSphere.PartneredCoedges = 36;
    padMinusSphere.Degree2Edges = 18;
    padMinusSphere.VertexBounds = MakeAabb(-1.300369867, 0.0, -1.732050808, 2.0,
                                           1.231502207, 1.732050808);
    padMinusSphere.FaceBounds = MakeAabb(-4.30156, -1.30334, -3.488995, 2.0,
                                         3.76634, 1.732050808);

    BooleanFingerprint sphereMinusPad;
    sphereMinusPad.Shells = 1;
    sphereMinusPad.Closed = true;
    sphereMinusPad.Type = BodyType::Solid;
    sphereMinusPad.Faces = 6;
    sphereMinusPad.Planes = 5;
    sphereMinusPad.Spheres = 1;
    sphereMinusPad.OuterLoops = 6;
    sphereMinusPad.InnerLoops = 1;
    sphereMinusPad.Edges = 13;
    sphereMinusPad.LineEdges = 8;
    sphereMinusPad.CircleEdges = 5;
    sphereMinusPad.Vertices = 10;
    sphereMinusPad.PartneredCoedges = 26;
    sphereMinusPad.Degree2Edges = 13;
    sphereMinusPad.ValidationIssues = 1;
    sphereMinusPad.VertexBounds = MakeAabb(-2.0, -1.30334, -1.732050808,
                                           0.645808122, 3.76634, 1.500384845);
    sphereMinusPad.FaceBounds = MakeAabb(-4.30156, -1.30334, -3.488995, 0.76812,
                                         3.76634, 1.580685);

    ExpectBooleanBothOrders(boolean::BooleanOp::Intersect, model, *pad, *sphere,
                            intersectFp);
    ExpectBooleanBothOrders(boolean::BooleanOp::Union, model, *pad, *sphere,
                            unionFp);
    ExpectBooleanOrdered(boolean::BooleanOp::Subtract, model, *pad, *sphere,
                         padMinusSphere);
    ExpectBooleanOrdered(boolean::BooleanOp::Subtract, model, *sphere, *pad,
                         sphereMinusPad);
}

}  // namespace
}  // namespace brep
