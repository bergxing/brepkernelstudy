#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/bool/Boolean.h"
#include "brep/bool/Classify.h"
#include "brep/bool/SphereRecognize.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace brep
{
namespace
{

TEST(SphereBoxBoolean, OctantIntersectValidates)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0,
                                    .Name = "S"});
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Intersect, model, *box, *sphere, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_EQ(result.body->shells[0]->faces.size(), 4u);
}

TEST(SphereBoxBoolean, RecognizesAnalyticSphere)
{
  Model model;
  Body* sphere = MakeSphere(model, SphereSpec{.Radius = 2.0, .Name = "S"});
  auto spec = boolean::recognize_analytic_sphere(*sphere, {});
  ASSERT_TRUE(spec.has_value());
  EXPECT_NEAR(spec->radius, 2.0, 1e-9);
}

TEST(SphereClassify, InOutOn)
{
  SphereSpec s{.Center = {0, 0, 0}, .Radius = 1.0};
  EXPECT_EQ(boolean::classify_point_in_sphere(s, {0, 0, 0}, 1e-7),
            boolean::SolidClass::In);
  EXPECT_EQ(boolean::classify_point_in_sphere(s, {2, 0, 0}, 1e-7),
            boolean::SolidClass::Out);
  EXPECT_EQ(boolean::classify_point_in_sphere(s, {1, 0, 0}, 1e-7),
            boolean::SolidClass::On);
}

}  // namespace
}  // namespace brep
