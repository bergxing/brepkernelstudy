#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/ops/profile.hpp"
#include "brep/validate.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace brep {
namespace {

TEST(PlanarBoolean, LExtrudeSubtractBoxValidates) {
  Model model;

  ops::ExtrudeSpec spec;
  spec.name = "L";
  spec.distance = 2.0;
  spec.plane = Plane::xz_y_up();
  // L-profile in UV (X,Z): not an AABB → prism path, not make_box.
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

  Body* box = make_box(
      model, BoxSpec{.min = {1.5, 0, 0}, .max = {3, 2, 1}, .name = "cutter"});
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  ASSERT_NE(eval, nullptr);
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *prism, *box, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(validate_body(*result.body).ok());
  EXPECT_GT(result.body->shells[0]->faces.size(), 6u);
}

TEST(PlanarBoolean, BoxIntersectLExtrudeValidates) {
  Model model;

  ops::ExtrudeSpec spec;
  spec.name = "L";
  spec.distance = 2.0;
  spec.plane = Plane::xz_y_up();
  spec.profile.outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::extrude(model, spec);
  Body* box = make_box(
      model, BoxSpec{.min = {0, 0, 0}, .max = {2, 2, 2}, .name = "B"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Intersect, model, *box, *prism, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(validate_body(*result.body).ok());
}

TEST(PlanarBoolean, LExtrudeUnionBoxValidates) {
  Model model;
  ops::ExtrudeSpec spec;
  spec.name = "L";
  spec.distance = 2.0;
  spec.plane = Plane::xz_y_up();
  spec.profile.outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::extrude(model, spec);
  Body* box = make_box(
      model, BoxSpec{.min = {2, 0, 0}, .max = {4, 2, 2}, .name = "B"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *prism, *box, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(validate_body(*result.body).ok());
}

}  // namespace
}  // namespace brep
