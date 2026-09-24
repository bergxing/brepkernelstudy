#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"
#include "brep/Model.h"
#include "brep/Validate.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/feat/BezierOps.h"

#include <cmath>
#include <gtest/gtest.h>

namespace brep
{
namespace
{

BezierSpec SampleSpec()
{
    return BezierSpec{
        .Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
        .Name = "bezier",
    };
}

TEST(BezierWire, MakeAndValidate)
{
    Model model;
    Body* body = MakeBezierWire(model, SampleSpec());
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->Type, BodyType::Wire);
    ASSERT_EQ(body->WireEdges.size(), 1u);
    EXPECT_TRUE(body->Shells.empty());

    Edge* edge = body->WireEdges[0];
    ASSERT_NE(edge, nullptr);
    ASSERT_NE(edge->Curve, nullptr);
    EXPECT_EQ(edge->Curve->Kind(), CurveKind::Bezier);
    ASSERT_NE(edge->V0, nullptr);
    ASSERT_NE(edge->V1, nullptr);

    EXPECT_TRUE(ValidateBody(*body).Ok());
}

TEST(BezierWire, EmptyWireEdgesFailsValidate)
{
    Model model;
    Body* body = model.MakeBody(BodyType::Wire, "empty_wire");
    ASSERT_NE(body, nullptr);
    EXPECT_TRUE(body->WireEdges.empty());

    const ValidationReport report = ValidateBody(*body);
    EXPECT_FALSE(report.Ok());
}

TEST(BezierWire, ExtractEdgesAndEmptyFaces)
{
    Model model;
    const BezierSpec spec = SampleSpec();
    Body* body = MakeBezierWire(model, spec);
    ASSERT_NE(body, nullptr);

    const EdgeMesh edges = ExtractEdges(*body);
    const std::size_t segmentCount = edges.Positions.size() / 2;
    EXPECT_GE(segmentCount, 32u);
    ASSERT_GE(edges.Positions.size(), 2u);
    EXPECT_NEAR((edges.Positions.front() - spec.P0()).norm(), 0.0, 1e-9);
    EXPECT_NEAR((edges.Positions.back() - spec.P3()).norm(), 0.0, 1e-9);

    const TriangleMesh faces = TessellateBody(*body);
    EXPECT_TRUE(faces.Vertices.empty());
    EXPECT_TRUE(faces.Indices.empty());
}

TEST(BezierWire, RemoveBodyPurgesWireSubgraph)
{
    Model solidOnly;
    MakeBox(solidOnly, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
    const ModelPoolStats solidStats = solidOnly.PoolStats();

    Model model;
    Body* keep =
        MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
    Body* wire = MakeBezierWire(model, SampleSpec());
    ASSERT_NE(keep, nullptr);
    ASSERT_NE(wire, nullptr);

    ASSERT_TRUE(model.RemoveBody(wire->Guid));
    EXPECT_EQ(model.PoolStats().Bodies, 1u);
    EXPECT_EQ(model.Bodies().front().get(), keep);

    const ModelPoolStats after = model.PoolStats();
    EXPECT_EQ(after.Vertices, solidStats.Vertices);
    EXPECT_EQ(after.Edges, solidStats.Edges);
    EXPECT_EQ(after.Curves, solidStats.Curves);
    EXPECT_EQ(after.Points, solidStats.Points);
    EXPECT_TRUE(ValidateBody(*keep).Ok());
}

TEST(BezierOps, ElevatePreservesEndpoints)
{
    BezierSpec spec{.Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}};
    BezierSpec up = ElevateBezierDegree(spec);
    ASSERT_EQ(up.Cvs.size(), 5u);
    EXPECT_EQ(up.Degree, 4);
    EXPECT_NEAR((up.Cvs.front() - spec.Cvs.front()).norm(), 0.0, 1e-12);
    EXPECT_NEAR((up.Cvs.back() - spec.Cvs.back()).norm(), 0.0, 1e-12);
    BezierCurve a(spec.Cvs);
    BezierCurve b(up.Cvs);
    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        EXPECT_NEAR((a.Eval(t) - b.Eval(t)).norm(), 0.0, 1e-9) << t;
    }
}

TEST(BezierOps, EnforceG1MakesCollinear)
{
    BezierSpec spec{
        .Cvs = {{0, 0, 0},
                {0, 1, 0},
                {1, 1, 0},
                {1, 0, 0},
                {2, 1, 0},
                {3, 1, 0},
                {3, 0, 0}},
        .Degree = 3,
        .SegmentCount = 2,
    };
    EnforceBezierG1(spec, 1, /*moveIn=*/true);
    const Vector3d in = spec.Cvs[3] - spec.Cvs[2];
    const Vector3d out = spec.Cvs[4] - spec.Cvs[3];
    EXPECT_NEAR(in.cross(out).norm(), 0.0, 1e-9);
}

TEST(BezierWire, LineAndQuadratic)
{
    Model model;
    BezierSpec line{.Cvs = {{0, 0, 0}, {2, 0, 0}},
                    .Degree = 1,
                    .SegmentCount = 1,
                    .Name = "line"};
    Body* lineBody = MakeBezierWire(model, line);
    ASSERT_NE(lineBody, nullptr);
    EXPECT_EQ(lineBody->WireEdges.size(), 1u);
    EXPECT_TRUE(ValidateBody(*lineBody).Ok());

    BezierSpec parabola{.Cvs = {{0, 0, 0}, {1, 1, 0}, {2, 0, 0}},
                        .Degree = 2,
                        .SegmentCount = 1,
                        .Name = "parabola"};
    Body* quad = MakeBezierWire(model, parabola);
    ASSERT_NE(quad, nullptr);
    EXPECT_TRUE(ValidateBody(*quad).Ok());
}

TEST(BezierWire, RationalWeightsAffectEval)
{
    Model model;
    BezierSpec spec{
        .Cvs = {{1, 0, 0}, {1, 1, 0}, {0, 1, 0}},
        .Weights = {1.0, std::sqrt(2.0) * 0.5, 1.0},
        .Degree = 2,
        .SegmentCount = 1,
        .Name = "arc",
    };
    Body* body = MakeBezierWire(model, spec);
    ASSERT_NE(body, nullptr);
    ASSERT_FALSE(body->WireEdges.empty());
    const Point3d mid = body->WireEdges[0]->Curve->Eval(0.5);
    const double w1 = std::sqrt(2.0) * 0.5;
    EXPECT_NEAR(mid.x(), w1, 1e-9);
    EXPECT_NEAR(mid.y(), w1, 1e-9);
}

TEST(BezierWire, MultiSegmentTwoEdges)
{
    Model model;
    BezierSpec spec{
        .Cvs = {{0, 0, 0},
                {0, 1, 0},
                {1, 1, 0},
                {1, 0, 0},
                {2, 0, 0},
                {2, -1, 0},
                {3, 0, 0}},
        .Degree = 3,
        .SegmentCount = 2,
        .Name = "poly",
    };
    Body* body = MakeBezierWire(model, spec);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->WireEdges.size(), 2u);
}

}  // namespace
}  // namespace brep
