#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/ops/Profile.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

// ponytail: general planar imprint does not yet split concave L-prism faces (M5).
TEST(PlanarBoolean, DISABLED_LExtrudeSubtractBoxValidates)
{
  Model model;

  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.Distance = 2.0;
  spec.Plane = Plane::XzYUp();
  spec.Profile.Outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };

  Body* prism = ops::Extrude(model, spec);
  ASSERT_NE(prism, nullptr);
  ASSERT_GT(prism->Shells[0]->Faces.size(), 6u);

  Body* box =
      MakeBox(model, BoxSpec{.Min = {1.5, 0, 0}, .Max = {3, 2, 1}, .Name = "cutter"});
  ASSERT_NE(box, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *prism, *box, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  EXPECT_GT(result.OutputBody->Shells[0]->Faces.size(), 6u);
}

TEST(PlanarBoolean, DISABLED_BoxIntersectLExtrudeValidates)
{
  Model model;

  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.Distance = 2.0;
  spec.Plane = Plane::XzYUp();
  spec.Profile.Outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::Extrude(model, spec);
  Body* box =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "B"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *box, *prism, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

TEST(PlanarBoolean, LExtrudeUnionBoxValidates)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.Distance = 2.0;
  spec.Plane = Plane::XzYUp();
  spec.Profile.Outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::Extrude(model, spec);
  Body* box =
      MakeBox(model, BoxSpec{.Min = {2, 0, 0}, .Max = {4, 2, 2}, .Name = "B"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *prism, *box, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

}  // namespace
}  // namespace brep
