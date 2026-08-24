#include "api/Core.h"
#include "api/Modeling.h"

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

  const auto med = boolean::probe_face_pair_intersections(
      *a, *b, spatial::BuildQuality::Median);
  const auto sah = boolean::probe_face_pair_intersections(
      *a, *b, spatial::BuildQuality::Sah);
  EXPECT_EQ(med.candidate_pairs, 0u);
  EXPECT_EQ(sah.candidate_pairs, 0u);
  EXPECT_EQ(med.nonempty_intersects, 0u);
  EXPECT_EQ(sah.nonempty_intersects, 0u);
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

  // Sah is the preferred quality for general broad-phase; Median remains for
  // A/B comparison and cheap builds (see Broadphase.h).
  const auto probe = boolean::probe_face_pair_intersections(
      *a, *b, spatial::BuildQuality::Sah);
  EXPECT_GT(probe.candidate_pairs, 0u);
  EXPECT_GT(probe.attempted_intersects, 0u);
  EXPECT_GT(probe.nonempty_intersects, 0u);
  EXPECT_EQ(probe.quality, spatial::BuildQuality::Sah);
}

TEST(BooleanBroadphase, UnsupportedEvaluatorMentionsBroadphase)
{
  Model model;
  // L-prism ∪ sphere: no specialized path → soft-fail with broadphase note.
  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.distance = 1.0;
  spec.plane = Plane::xz_y_up();
  spec.profile.outer = {
      Point2d{0, 0}, Point2d{2, 0}, Point2d{2, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::extrude(model, spec);
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {3, 0, 0}, .Radius = 0.5, .Name = "S"});
  ASSERT_NE(prism, nullptr);
  ASSERT_NE(sphere, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *prism, *sphere, {});
  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.diagnostics.find("broadphase"), std::string::npos)
      << result.diagnostics;
  EXPECT_NE(result.diagnostics.find("Sah"), std::string::npos)
      << result.diagnostics;
}

}  // namespace
}  // namespace brep
