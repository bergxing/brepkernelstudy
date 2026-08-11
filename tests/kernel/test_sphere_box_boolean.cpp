#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/bool/classify.hpp"
#include "brep/bool/sphere_recognize.hpp"
#include "brep/validate.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace brep {
namespace {

TEST(SphereBoxBoolean, OctantIntersectValidates) {
  Model model;
  Body* sphere =
      make_sphere(model, SphereSpec{.center = {0, 0, 0}, .radius = 1.0,
                                    .name = "S"});
  Body* box = make_box(
      model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "B"});
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Intersect, model, *box, *sphere, {});
  EXPECT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  EXPECT_TRUE(validate_body(*result.body).ok());
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_EQ(result.body->shells[0]->faces.size(), 4u);
}

TEST(SphereBoxBoolean, RecognizesAnalyticSphere) {
  Model model;
  Body* sphere = make_sphere(model, SphereSpec{.radius = 2.0, .name = "S"});
  auto spec = boolean::recognize_analytic_sphere(*sphere, {});
  ASSERT_TRUE(spec.has_value());
  EXPECT_NEAR(spec->radius, 2.0, 1e-9);
}

TEST(SphereClassify, InOutOn) {
  SphereSpec s{.center = {0, 0, 0}, .radius = 1.0};
  EXPECT_EQ(boolean::classify_point_in_sphere(s, {0, 0, 0}, 1e-7),
            boolean::SolidClass::In);
  EXPECT_EQ(boolean::classify_point_in_sphere(s, {2, 0, 0}, 1e-7),
            boolean::SolidClass::Out);
  EXPECT_EQ(boolean::classify_point_in_sphere(s, {1, 0, 0}, 1e-7),
            boolean::SolidClass::On);
}

}  // namespace
}  // namespace brep
