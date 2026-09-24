#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/bool/Broadphase.h"
#include "brep/ops/Profile.h"
#include "brep/spatial/FaceBvh.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(BooleanBroadphase, SeparatedBodiesHaveZeroCandidates)
{
  Model model;
  Body* a =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b =
      MakeBox(model, BoxSpec{.Min = {4, 0, 0}, .Max = {5, 1, 1}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  const auto med =
      boolean::ProbeFacePairIntersections(*a, *b, spatial::BuildQuality::Median);
  const auto sah =
      boolean::ProbeFacePairIntersections(*a, *b, spatial::BuildQuality::Sah);
  EXPECT_EQ(med.CandidatePairs, 0u);
  EXPECT_EQ(sah.CandidatePairs, 0u);
  EXPECT_EQ(med.NonemptyIntersects, 0u);
  EXPECT_EQ(sah.NonemptyIntersects, 0u);
}

TEST(BooleanBroadphase, OverlappingBoxesProbePlanePlaneHits)
{
  Model model;
  Body* a =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* b =
      MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  const auto probe =
      boolean::ProbeFacePairIntersections(*a, *b, spatial::BuildQuality::Sah);
  EXPECT_GT(probe.CandidatePairs, 0u);
  EXPECT_GT(probe.AttemptedIntersects, 0u);
  EXPECT_GT(probe.NonemptyIntersects, 0u);
  EXPECT_EQ(probe.Quality, spatial::BuildQuality::Sah);
}

TEST(BooleanBroadphase, DisjointPrismUnionSphereSucceeds)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.Distance = 1.0;
  spec.Plane = Plane::XzYUp();
  spec.Profile.Outer = {
      Point2d{0, 0}, Point2d{2, 0}, Point2d{2, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::Extrude(model, spec);
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {3, 0, 0}, .Radius = 0.5, .Name = "S"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(sphere, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *prism, *sphere, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  ASSERT_NE(result.OutputBody, nullptr);
}

}  // namespace
}  // namespace brep
