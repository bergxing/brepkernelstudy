#include "brep/Model.h"
#include "brep/Validate.h"
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
