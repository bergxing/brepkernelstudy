#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/bool/Evaluator.h"
#include "brep/feat/NurbsCurveFeature.h"

#include <gtest/gtest.h>
#include <variant>

namespace brep
{
namespace
{

NurbsCurveSpec SampleFiveCvSpec()
{
    return NurbsCurveSpec{
        .Cvs = {{0, 0, 0}, {1, 0, 0}, {2, 1, 0}, {3, 1, 0}, {4, 0, 0}},
        .Tolerance = 1e-7,
        .Name = "nurbs",
    };
}

TEST(NurbsFeature, AddNurbsCurveCreatesWireBody)
{
    auto doc = Document::Create("nurbs_add",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    Body* body = part.AddNurbsCurve(SampleFiveCvSpec());
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(part.Model().Bodies().size(), 1u);
    EXPECT_EQ(body->Type, BodyType::Wire);
    ASSERT_EQ(body->WireEdges.size(), 1u);

    auto* feature = part.Features().FindByBody(body->Guid);
    ASSERT_NE(feature, nullptr);
    EXPECT_EQ(feature->TypeName(), "NurbsCurve");

    auto prim = feature->ToPrimitiveSpec(part.Parameters());
    ASSERT_TRUE(prim.has_value());
    const auto* nurbs = std::get_if<NurbsCurveSpec>(&*prim);
    ASSERT_NE(nurbs, nullptr);
    EXPECT_EQ(nurbs->Cvs.size(), 5u);
    EXPECT_EQ(nurbs->Knots.size(), 9u);
}

}  // namespace
}  // namespace brep
