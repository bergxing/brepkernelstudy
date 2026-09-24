#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"

#include <gtest/gtest.h>

#include <memory>

namespace brep
{
namespace
{

TEST(BooleanSkeleton, TypesAndContextDefaults)
{
  EXPECT_EQ(static_cast<int>(boolean::BooleanOp::Union), 0);
  EXPECT_EQ(static_cast<int>(boolean::BooleanOp::Subtract), 1);
  EXPECT_EQ(static_cast<int>(boolean::BooleanOp::Intersect), 2);

  const boolean::BooleanContext ctx;
  EXPECT_GT(ctx.fuzzy, 0.0);
  EXPECT_GT(ctx.tol_3d, 0.0);
  EXPECT_GT(ctx.tol_2d, 0.0);
}

TEST(BooleanSkeleton, StubRejectsUnsupportedCombinations)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "a"});
  Body* b = MakeBox(model, BoxSpec{.Min = {0.5, 0.5, 0.5}, .Max = {1.5, 1.5, 1.5},
                                    .Name = "b"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  std::unique_ptr<boolean::IBooleanEvaluator> eval =
      boolean::MakeStubBooleanEvaluator();
  ASSERT_NE(eval, nullptr);

  const boolean::BooleanContext ctx;
  for (const auto op : {boolean::BooleanOp::Union, boolean::BooleanOp::Subtract,
                        boolean::BooleanOp::Intersect})
  {
    const boolean::BooleanResult result = eval->Evaluate(op, model, *a, *b, ctx);
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.OutputBody, nullptr);
    EXPECT_FALSE(result.Diagnostics.empty()) << "op=" << static_cast<int>(op);
  }
}

TEST(BooleanSkeleton, StubRejectsSphereBoxPair)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}});
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {1, 1, 1}, .Radius = 0.75});
  ASSERT_NE(box, nullptr);
  ASSERT_NE(sphere, nullptr);

  auto eval = boolean::MakeStubBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *box, *sphere, {});
  EXPECT_FALSE(result.Ok());
  EXPECT_NE(result.Diagnostics.find("unsupported"), std::string::npos);
}

}  // namespace
}  // namespace brep
