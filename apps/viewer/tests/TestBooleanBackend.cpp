#include "bootstrap/ApplicationContainer.h"
#include "bootstrap/BooleanBackend.h"
#include "bootstrap/KernelServices.h"

#include "api/Modeling.h"

#include <gtest/gtest.h>

namespace brep::viewer::bootstrap
{
namespace
{

boolean::BooleanResult EvaluateDefaultBoxUnion(boolean::IBooleanEvaluator& evaluator)
{
  auto doc = Document::Create(
      "EvalTest", CreateBooleanEvaluator(BooleanBackend::Default));
  Part& part = doc->AddPart();
  BoxSpec spec_a{
      .Min = {0, 0, 0},
      .Max = {1, 1, 1},
      .Name = "a",
  };
  BoxSpec spec_b{
      .Min = {0.5, 0, 0},
      .Max = {1.5, 1, 1},
      .Name = "b",
  };
  Body* body_a = part.AddBox(spec_a);
  Body* body_b = part.AddBox(spec_b);
  return evaluator.Evaluate(boolean::BooleanOp::Union, part.Model(), *body_a,
                            *body_b, {});
}

TEST(BooleanBackend, DefaultBackendProducesUnionOutput)
{
  AppConfig config;
  config.booleanBackend = BooleanBackend::Default;
  const auto container = BuildApplicationContainer(config);
  ASSERT_NE(container, nullptr);

  auto evaluator = ResolveBooleanEvaluator(container);
  ASSERT_NE(evaluator, nullptr);
  const auto result = EvaluateDefaultBoxUnion(*evaluator);
  EXPECT_NE(result.OutputBody, nullptr);
}

TEST(BooleanBackend, StubBackendFailsSoftOnUnion)
{
  AppConfig config;
  config.booleanBackend = BooleanBackend::Stub;
  const auto container = BuildApplicationContainer(config);
  auto evaluator = ResolveBooleanEvaluator(container);
  ASSERT_NE(evaluator, nullptr);

  const auto result = EvaluateDefaultBoxUnion(*evaluator);
  EXPECT_EQ(result.OutputBody, nullptr);
  EXPECT_FALSE(result.Diagnostics.empty());
}

TEST(BooleanBackend, OcctFallsBackToDefault)
{
  AppConfig config;
  config.booleanBackend = BooleanBackend::Occt;
  const auto container = BuildApplicationContainer(config);
  auto evaluator = ResolveBooleanEvaluator(container);
  ASSERT_NE(evaluator, nullptr);

  const auto result = EvaluateDefaultBoxUnion(*evaluator);
  EXPECT_NE(result.OutputBody, nullptr);
}

TEST(BooleanBackend, NamedStubResolveDistinctFromDefault)
{
  AppConfig config;
  config.booleanBackend = BooleanBackend::Default;
  const auto container = BuildApplicationContainer(config);

  auto defaultEval = ResolveNamedBooleanEvaluator(container, "default");
  auto stubEval = ResolveNamedBooleanEvaluator(container, "stub");
  ASSERT_NE(defaultEval, nullptr);
  ASSERT_NE(stubEval, nullptr);
  EXPECT_NE(defaultEval.get(), stubEval.get());

  const auto ok = EvaluateDefaultBoxUnion(*defaultEval);
  const auto fail = EvaluateDefaultBoxUnion(*stubEval);
  EXPECT_NE(ok.OutputBody, nullptr);
  EXPECT_EQ(fail.OutputBody, nullptr);
}

}  // namespace
}  // namespace brep::viewer::bootstrap
