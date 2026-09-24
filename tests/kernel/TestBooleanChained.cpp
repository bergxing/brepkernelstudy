#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(BooleanChained, BoxIntersectBoxThenSubtractBox)
{
  Model model;
  Body* boxA =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* boxB =
      MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  Body* boxC =
      MakeBox(model, BoxSpec{.Min = {1.5, 1.5, 0}, .Max = {2.5, 2.5, 1}, .Name = "C"});

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto mid =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *boxA, *boxB, {});
  ASSERT_TRUE(mid.Ok()) << mid.Diagnostics;
  ASSERT_NE(mid.OutputBody, nullptr);

  const auto finalResult =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *mid.OutputBody, *boxC, {});
  EXPECT_TRUE(finalResult.Ok()) << finalResult.Diagnostics;
  ASSERT_NE(finalResult.OutputBody, nullptr);
  EXPECT_EQ(finalResult.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*finalResult.OutputBody).Ok());
}

TEST(BooleanChained, SphereUnionThenSubtractDisjointBox)
{
    Model model;
    Body* sphereA = MakeSphere(
        model,
        SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "Sa"});
    Body* sphereB = MakeSphere(
        model,
        SphereSpec{.Center = {0, 0, 1}, .Radius = 1.0, .Name = "Sb"});
    Body* box = MakeBox(
        model,
        BoxSpec{.Min = {3, 3, 3}, .Max = {4, 4, 4}, .Name = "B"});

    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto mid = eval->Evaluate(boolean::BooleanOp::Union, model, *sphereA,
                                    *sphereB, {});
    ASSERT_TRUE(mid.Ok()) << mid.Diagnostics;
    ASSERT_NE(mid.OutputBody, nullptr);
    EXPECT_TRUE(ValidateBody(*mid.OutputBody).Ok());

    const auto finalResult = eval->Evaluate(
        boolean::BooleanOp::Subtract, model, *mid.OutputBody, *box, {});
    ASSERT_TRUE(finalResult.Ok()) << finalResult.Diagnostics;
    ASSERT_NE(finalResult.OutputBody, nullptr);
    EXPECT_EQ(finalResult.Mode, boolean::BooleanEvalMode::General);
    EXPECT_TRUE(ValidateBody(*finalResult.OutputBody).Ok());
}

}  // namespace
}  // namespace brep
