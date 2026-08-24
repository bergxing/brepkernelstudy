#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/bool/Boolean.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(SphereBoxBoolean, OctantSphereMinusBoxValidates)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0,
                                    .Name = "S"});
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  // Sphere is target (A), box is tool (B).
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok()) << "⅞-ball must validate";
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_EQ(result.body->shells[0]->faces.size(), 4u);
}

TEST(SphereBoxBoolean, OctantUnionValidates)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0,
                                    .Name = "S"});
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *sphere, *box, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
  // 6 box faces + 1 spherical patch
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_EQ(result.body->shells[0]->faces.size(), 7u);
}

TEST(SphereBoxBoolean, UntitledXlCornerUnionValidates)
{
  // Bodies from untitled.xl (box_copy_copy + sphere at max/max/min corner).
  Model model;
  Body* box = MakeBox(
      model, BoxSpec{.Min = {-2.38421, 0, 4.59474},
                     .Max = {-1.57147, 0.826297, 5.26894},
                     .Name = "box_copy_copy"});
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {-1.57147, 0.826297, 4.59474},
                        .Radius = 0.413149,
                        .Name = "sphere"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *box, *sphere, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
}

TEST(SphereBoxBoolean, SphereMinusBoxUnsupportedPoseSoftFails)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0,
                                    .Name = "S"});
  Body* box = MakeBox(
      model, BoxSpec{.Min = {2, 2, 2}, .Max = {3, 3, 3}, .Name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.diagnostics.empty());
}

TEST(SphereSphereBoolean, IntersectingUnionValidates)
{
  Model model;
  Body* a =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0,
                                    .Name = "Sa"});
  Body* b =
      MakeSphere(model, SphereSpec{.Center = {1.2, 0, 0}, .Radius = 1.0,
                                    .Name = "Sb"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_EQ(result.body->shells[0]->faces.size(), 2u);
}

TEST(SphereSphereBoolean, ContainedUnionReturnsLarger)
{
  Model model;
  Body* a =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 2.0,
                                    .Name = "Big"});
  Body* b =
      MakeSphere(model, SphereSpec{.Center = {0.2, 0, 0}, .Radius = 0.5,
                                    .Name = "Small"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
}

TEST(SphereSphereBoolean, SeparateUnionSoftFails)
{
  Model model;
  Body* a =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0,
                                    .Name = "Sa"});
  Body* b =
      MakeSphere(model, SphereSpec{.Center = {5, 0, 0}, .Radius = 1.0,
                                    .Name = "Sb"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.diagnostics.find("separate"), std::string::npos)
      << result.diagnostics;
}

}  // namespace
}  // namespace brep
