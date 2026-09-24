#include "api/Mesh.h"
#include "brep/Model.h"
#include "brep/Validate.h"
#include "brep/bool/TopologyCopy.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/feat/PrimitiveSpecs.h"

#include <gtest/gtest.h>

using namespace brep;

TEST(NurbsWire, OneEdgeThroughEnds)
{
    Model model;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{1, 0, 0}, Point3d{2, 1, 0},
                Point3d{3, 1, 0}, Point3d{4, 0, 0}};
    spec.Name = "nurbs";
    Body* body = MakeNurbsCurveWire(model, spec);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->Type, BodyType::Wire);
    ASSERT_EQ(body->WireEdges.size(), 1u);
    EXPECT_TRUE(body->Shells.empty());
    EXPECT_EQ(body->WireEdges[0]->Curve->Kind(), CurveKind::Nurbs);
    EXPECT_LT(body->WireEdges[0]->V0->Position().distance_to(spec.Cvs.front()),
              1e-12);
    EXPECT_LT(body->WireEdges[0]->V1->Position().distance_to(spec.Cvs.back()),
              1e-12);
    EXPECT_TRUE(ValidateBody(*body).Ok());
}

TEST(NurbsWire, InvalidSpecReturnsNull)
{
    Model model;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{}, Point3d{1, 0, 0}};
    EXPECT_EQ(MakeNurbsCurveWire(model, spec), nullptr);
}

TEST(NurbsWire, ExtractEdgesReachesEnds)
{
    Model model;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{0, 1, 0}, Point3d{1, 1, 0},
                Point3d{1, 0, 0}};
    Body* body = MakeNurbsCurveWire(model, spec);
    ASSERT_NE(body, nullptr);
    const EdgeMesh mesh = ExtractEdges(*body);
    ASSERT_GE(mesh.Positions.size(), 33u);
    EXPECT_LT(mesh.Positions.front().distance_to(spec.Cvs.front()), 1e-6);
    EXPECT_LT(mesh.Positions.back().distance_to(spec.Cvs.back()), 1e-6);
    const TriangleMesh faces = TessellateBody(*body);
    EXPECT_TRUE(faces.Indices.empty());
}

TEST(NurbsWire, CopyBodyPreservesNurbsCurve)
{
    Model model;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{0, 1, 0}, Point3d{1, 1, 0},
                Point3d{1, 0, 0}};
    spec.Weights = {1.0, 0.8, 0.9, 1.0};
    Body* body = MakeNurbsCurveWire(model, spec);
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(body->WireEdges.size(), 1u);
    ASSERT_NE(body->WireEdges[0]->Curve, nullptr);

    boolean::TopologyCopyContext ctx{model};
    Body* copied = boolean::CopyBodySubgraph(ctx, *body, "_cp");
    ASSERT_NE(copied, nullptr);
    ASSERT_EQ(copied->WireEdges.size(), 1u);
    ASSERT_NE(copied->WireEdges[0]->Curve, nullptr);
    EXPECT_EQ(copied->WireEdges[0]->Curve->Kind(), CurveKind::Nurbs);

    const auto& src =
        static_cast<const NurbsCurve&>(*body->WireEdges[0]->Curve);
    const auto& dst =
        static_cast<const NurbsCurve&>(*copied->WireEdges[0]->Curve);
    ASSERT_EQ(dst.Cvs().size(), src.Cvs().size());
    for (std::size_t i = 0; i < src.Cvs().size(); ++i)
    {
        EXPECT_LT(dst.Cvs()[i].distance_to(src.Cvs()[i]), 1e-12) << i;
    }
    EXPECT_EQ(dst.Weights(), src.Weights());
    EXPECT_EQ(dst.Knots(), src.Knots());
}
