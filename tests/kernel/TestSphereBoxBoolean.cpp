#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/bool/Classify.h"
#include "brep/bool/SphereRecognize.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <cmath>

namespace brep
{
namespace
{

TEST(SphereBoxBoolean, OctantIntersectValidates)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
  Body* box =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *box, *sphere, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  ASSERT_FALSE(result.OutputBody->Shells.empty());
  EXPECT_EQ(result.OutputBody->Shells[0]->Faces.size(), 4u);
}

TEST(SphereBoxBoolean, RecognizesAnalyticSphere)
{
  Model model;
  Body* sphere = MakeSphere(model, SphereSpec{.Radius = 2.0, .Name = "S"});
  auto spec = boolean::RecognizeAnalyticSphere(*sphere, {});
  ASSERT_TRUE(spec.has_value());
  EXPECT_NEAR(spec->Radius, 2.0, 1e-9);
}

TEST(SphereClassify, InOutOn)
{
  SphereSpec s{.Center = {0, 0, 0}, .Radius = 1.0};
  EXPECT_EQ(boolean::ClassifyPointInSphere(s, {0, 0, 0}, 1e-7), boolean::SolidClass::In);
  EXPECT_EQ(boolean::ClassifyPointInSphere(s, {2, 0, 0}, 1e-7), boolean::SolidClass::Out);
  EXPECT_EQ(boolean::ClassifyPointInSphere(s, {1, 0, 0}, 1e-7), boolean::SolidClass::On);
}

}  // namespace
}  // namespace brep
