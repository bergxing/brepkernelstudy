#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/bool/broadphase.hpp"
#include "brep/ops/profile.hpp"
#include "brep/spatial/face_bvh.hpp"

#include <gtest/gtest.h>

namespace brep {
namespace {

TEST(BooleanBroadphase, SeparatedBodiesHaveZeroCandidates) {
  Model model;
  Body* a =
      make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "A"});
  Body* b =
      make_box(model, BoxSpec{.min = {4, 0, 0}, .max = {5, 1, 1}, .name = "B"});
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

TEST(BooleanBroadphase, OverlappingBoxesProbePlanePlaneHits) {
  Model model;
  Body* a =
      make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 2, 2}, .name = "A"});
  Body* b =
      make_box(model, BoxSpec{.min = {1, 1, 1}, .max = {3, 3, 3}, .name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  // Sah is the preferred quality for general broad-phase; Median remains for
  // A/B comparison and cheap builds (see broadphase.hpp).
  const auto probe = boolean::probe_face_pair_intersections(
      *a, *b, spatial::BuildQuality::Sah);
  EXPECT_GT(probe.candidate_pairs, 0u);
  EXPECT_GT(probe.attempted_intersects, 0u);
  EXPECT_GT(probe.nonempty_intersects, 0u);
  EXPECT_EQ(probe.quality, spatial::BuildQuality::Sah);
}

TEST(BooleanBroadphase, UnsupportedEvaluatorMentionsBroadphase) {
  Model model;
  // L-prism ∪ sphere: no specialized path → soft-fail with broadphase note.
  ops::ExtrudeSpec spec;
  spec.name = "L";
  spec.distance = 1.0;
  spec.plane = Plane::xz_y_up();
  spec.profile.outer = {
      Point2d{0, 0}, Point2d{2, 0}, Point2d{2, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* prism = ops::extrude(model, spec);
  Body* sphere = make_sphere(
      model, SphereSpec{.center = {3, 0, 0}, .radius = 0.5, .name = "S"});
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
