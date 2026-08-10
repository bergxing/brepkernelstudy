#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace brep {
namespace {

TEST(BooleanSkeleton, TypesAndContextDefaults) {
  EXPECT_EQ(static_cast<int>(boolean::BooleanOp::Union), 0);
  EXPECT_EQ(static_cast<int>(boolean::BooleanOp::Subtract), 1);
  EXPECT_EQ(static_cast<int>(boolean::BooleanOp::Intersect), 2);

  const boolean::BooleanContext ctx;
  EXPECT_GT(ctx.fuzzy, 0.0);
  EXPECT_GT(ctx.tol_3d, 0.0);
  EXPECT_GT(ctx.tol_2d, 0.0);
}

TEST(BooleanSkeleton, StubRejectsUnsupportedCombinations) {
  Model model;
  Body* a = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "a"});
  Body* b = make_box(model, BoxSpec{.min = {0.5, 0.5, 0.5}, .max = {1.5, 1.5, 1.5},
                                    .name = "b"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  std::unique_ptr<boolean::IBooleanEvaluator> eval =
      boolean::make_stub_boolean_evaluator();
  ASSERT_NE(eval, nullptr);

  const boolean::BooleanContext ctx;
  for (const auto op : {boolean::BooleanOp::Union, boolean::BooleanOp::Subtract,
                        boolean::BooleanOp::Intersect}) {
    const boolean::BooleanResult result = eval->evaluate(op, model, *a, *b, ctx);
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.body, nullptr);
    EXPECT_FALSE(result.diagnostics.empty()) << "op=" << static_cast<int>(op);
  }
}

TEST(BooleanSkeleton, StubRejectsSphereBoxPair) {
  Model model;
  Body* box = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 2, 2}});
  Body* sphere =
      make_sphere(model, SphereSpec{.center = {1, 1, 1}, .radius = 0.75});
  ASSERT_NE(box, nullptr);
  ASSERT_NE(sphere, nullptr);

  auto eval = boolean::make_stub_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *box, *sphere, {});
  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.diagnostics.find("unsupported"), std::string::npos);
}

}  // namespace
}  // namespace brep
