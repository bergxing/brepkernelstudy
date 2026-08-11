#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/validate.hpp"

#include <gtest/gtest.h>

namespace brep {
namespace {

TEST(SphereBoxBoolean, OctantSphereMinusBoxValidates) {
  Model model;
  Body* sphere =
      make_sphere(model, SphereSpec{.center = {0, 0, 0}, .radius = 1.0,
                                    .name = "S"});
  Body* box = make_box(
      model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  // Sphere is target (A), box is tool (B).
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(validate_body(*result.body).ok()) << "⅞-ball must validate";
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_EQ(result.body->shells[0]->faces.size(), 4u);
}

TEST(SphereBoxBoolean, SphereMinusBoxUnsupportedPoseSoftFails) {
  Model model;
  Body* sphere =
      make_sphere(model, SphereSpec{.center = {0, 0, 0}, .radius = 1.0,
                                    .name = "S"});
  Body* box = make_box(
      model, BoxSpec{.min = {2, 2, 2}, .max = {3, 3, 3}, .name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.diagnostics.empty());
}

TEST(SphereSphereBoolean, IntersectingUnionValidates) {
  Model model;
  Body* a =
      make_sphere(model, SphereSpec{.center = {0, 0, 0}, .radius = 1.0,
                                    .name = "Sa"});
  Body* b =
      make_sphere(model, SphereSpec{.center = {1.2, 0, 0}, .radius = 1.0,
                                    .name = "Sb"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(validate_body(*result.body).ok());
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_EQ(result.body->shells[0]->faces.size(), 2u);
}

TEST(SphereSphereBoolean, ContainedUnionReturnsLarger) {
  Model model;
  Body* a =
      make_sphere(model, SphereSpec{.center = {0, 0, 0}, .radius = 2.0,
                                    .name = "Big"});
  Body* b =
      make_sphere(model, SphereSpec{.center = {0.2, 0, 0}, .radius = 0.5,
                                    .name = "Small"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(validate_body(*result.body).ok());
}

TEST(SphereSphereBoolean, SeparateUnionSoftFails) {
  Model model;
  Body* a =
      make_sphere(model, SphereSpec{.center = {0, 0, 0}, .radius = 1.0,
                                    .name = "Sa"});
  Body* b =
      make_sphere(model, SphereSpec{.center = {5, 0, 0}, .radius = 1.0,
                                    .name = "Sb"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.diagnostics.find("separate"), std::string::npos)
      << result.diagnostics;
}

}  // namespace
}  // namespace brep
