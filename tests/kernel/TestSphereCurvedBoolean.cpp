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

TEST(SphereSphereBoolean, OffsetZUnionUsesGeneralPipeline)
{
    Model model;
    Body* a = MakeSphere(
        model,
        SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "Sa"});
    Body* b = MakeSphere(
        model,
        SphereSpec{.Center = {0, 0, 1}, .Radius = 1.0, .Name = "Sb"});
    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto result =
        eval->Evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
    ASSERT_TRUE(result.Ok()) << result.Diagnostics;
    ASSERT_NE(result.OutputBody, nullptr);
    EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
    EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
    ASSERT_NE(result.OutputBody->OuterShell(), nullptr);
    EXPECT_EQ(result.OutputBody->OuterShell()->Faces.size(), 2U);
}

TEST(SphereSphereBoolean, OffsetZIntersectUsesGeneralPipeline)
{
    Model model;
    Body* a = MakeSphere(
        model,
        SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "Sa"});
    Body* b = MakeSphere(
        model,
        SphereSpec{.Center = {0, 0, 1}, .Radius = 1.0, .Name = "Sb"});
    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto result =
        eval->Evaluate(boolean::BooleanOp::Intersect, model, *a, *b, {});
    ASSERT_TRUE(result.Ok()) << result.Diagnostics;
    ASSERT_NE(result.OutputBody, nullptr);
    EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
    EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
    ASSERT_NE(result.OutputBody->OuterShell(), nullptr);
    EXPECT_EQ(result.OutputBody->OuterShell()->Faces.size(), 2U);
}

TEST(SphereBoxBoolean, OctantSphereMinusBoxValidates)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
  Body* box =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box,
                     {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  ASSERT_FALSE(result.OutputBody->Shells.empty());
  EXPECT_EQ(result.OutputBody->Shells[0]->Faces.size(), 4u);
}

TEST(SphereBoxBoolean, OctantUnionValidates)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
  Body* box =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *sphere, *box, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  ASSERT_FALSE(result.OutputBody->Shells.empty());
  EXPECT_EQ(result.OutputBody->Shells[0]->Faces.size(), 7u);
}

TEST(SphereBoxBoolean, UntitledXlCornerUnionValidates)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {-2.38421, 0, 4.59474},
                                     .Max = {-1.57147, 0.826297, 5.26894},
                                     .Name = "box_copy_copy"});
  Body* sphere = MakeSphere(model, SphereSpec{.Center = {-1.57147, 0.826297, 4.59474},
                                              .Radius = 0.413149,
                                              .Name = "sphere"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *box, *sphere, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

TEST(SphereBoxBoolean, DisjointSphereMinusBoxReturnsSphere)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
  Body* box =
      MakeBox(model, BoxSpec{.Min = {2, 2, 2}, .Max = {3, 3, 3}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box,
                     {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

TEST(SphereSphereBoolean, DISABLED_IntersectingUnionValidates)
{
  Model model;
  Body* a =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "Sa"});
  Body* b =
      MakeSphere(model, SphereSpec{.Center = {1.2, 0, 0}, .Radius = 1.0, .Name = "Sb"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  ASSERT_FALSE(result.OutputBody->Shells.empty());
  EXPECT_EQ(result.OutputBody->Shells[0]->Faces.size(), 2u);
}

TEST(SphereSphereBoolean, DISABLED_ContainedUnionReturnsLarger)
{
  Model model;
  Body* a =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 2.0, .Name = "Big"});
  Body* b =
      MakeSphere(model, SphereSpec{.Center = {0.2, 0, 0}, .Radius = 0.5, .Name = "Small"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

TEST(SphereSphereBoolean, DISABLED_SeparateUnionSoftFails)
{
  Model model;
  Body* a =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "Sa"});
  Body* b =
      MakeSphere(model, SphereSpec{.Center = {5, 0, 0}, .Radius = 1.0, .Name = "Sb"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_FALSE(result.Ok());
  EXPECT_NE(result.Diagnostics.find("separate"), std::string::npos)
      << result.Diagnostics;
}

}  // namespace
}  // namespace brep
