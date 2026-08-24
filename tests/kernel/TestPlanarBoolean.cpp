#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/bool/Boolean.h"
#include "brep/ops/Profile.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <memory>

namespace brep
{
namespace
{

TEST(PlanarBoolean, LExtrudeSubtractBoxValidates)
{
  Model model;

  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.distance = 2.0;
  spec.plane = Plane::xz_y_up();
  // L-profile in UV (X,Z): not an AABB → prism path, not MakeBox.
  spec.profile.outer = {
      Point2d{0, 0},
      Point2d{3, 0},
      Point2d{3, 1},
      Point2d{1, 1},
      Point2d{1, 2},
      Point2d{0, 2},
  };

  Body* prism = ops::extrude(model, spec);
  ASSERT_NE(prism, nullptr);
  ASSERT_GT(prism->shells[0]->faces.size(), 6u);

  Body* box = MakeBox(
      model, BoxSpec{.Min = {1.5, 0, 0}, .Max = {3, 2, 1}, .Name = "cutter"});
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  ASSERT_NE(eval, nullptr);
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *prism, *box, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
  EXPECT_GT(result.body->shells[0]->faces.size(), 6u);
}

TEST(PlanarBoolean, BoxIntersectLExtrudeValidates)
{
  Model model;

  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.distance = 2.0;
  spec.plane = Plane::xz_y_up();
  spec.profile.outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::extrude(model, spec);
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "B"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Intersect, model, *box, *prism, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
}

TEST(PlanarBoolean, LExtrudeUnionBoxValidates)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.distance = 2.0;
  spec.plane = Plane::xz_y_up();
  spec.profile.outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::extrude(model, spec);
  Body* box = MakeBox(
      model, BoxSpec{.Min = {2, 0, 0}, .Max = {4, 2, 2}, .Name = "B"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *prism, *box, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(ValidateBody(*result.body).Ok());
}

}  // namespace
}  // namespace brep
