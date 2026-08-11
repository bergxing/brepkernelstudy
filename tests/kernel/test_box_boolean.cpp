#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/validate.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

namespace brep {
namespace {

[[nodiscard]] double aabb_volume(const Point3d& mn, const Point3d& mx) {
  return (mx.x() - mn.x()) * (mx.y() - mn.y()) * (mx.z() - mn.z());
}

[[nodiscard]] std::pair<Point3d, Point3d> body_aabb(const Body& body) {
  Point3d mn{1e300, 1e300, 1e300};
  Point3d mx{-1e300, -1e300, -1e300};
  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (const Face* face : shell->faces) {
      if (!face) continue;
      for (const Loop* loop : face->loops) {
        if (!loop) continue;
        loop->for_each_coedge([&](const CoEdge& ce) {
          if (Vertex* v = ce.from()) {
            const Point3d& p = v->position();
            mn = Point3d{std::min(mn.x(), p.x()), std::min(mn.y(), p.y()),
                         std::min(mn.z(), p.z())};
            mx = Point3d{std::max(mx.x(), p.x()), std::max(mx.y(), p.y()),
                         std::max(mx.z(), p.z())};
          }
        });
      }
    }
  }
  return {mn, mx};
}

TEST(BoxBoolean, UnionOverlappingValidates) {
  Model model;
  Body* a = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 1, 1}, .name = "A"});
  Body* b = make_box(model, BoxSpec{.min = {1, 0, 0}, .max = {3, 1, 1}, .name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  ASSERT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  const auto report = validate_body(*result.body);
  EXPECT_TRUE(report.ok());
  auto [mn, mx] = body_aabb(*result.body);
  EXPECT_NEAR(mn.x(), 0.0, 1e-9);
  EXPECT_NEAR(mx.x(), 3.0, 1e-9);
  EXPECT_NEAR(aabb_volume(mn, mx), 3.0, 1e-9);
}

TEST(BoxBoolean, IntersectOverlappingIsBox) {
  Model model;
  Body* a = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 2, 2}, .name = "A"});
  Body* b = make_box(model, BoxSpec{.min = {1, 1, 1}, .max = {3, 3, 3}, .name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Intersect, model, *a, *b, {});
  ASSERT_TRUE(result.ok()) << result.diagnostics;
  EXPECT_TRUE(validate_body(*result.body).ok());
  auto [mn, mx] = body_aabb(*result.body);
  EXPECT_NEAR(mn.x(), 1.0, 1e-9);
  EXPECT_NEAR(mn.y(), 1.0, 1e-9);
  EXPECT_NEAR(mn.z(), 1.0, 1e-9);
  EXPECT_NEAR(mx.x(), 2.0, 1e-9);
  EXPECT_NEAR(mx.y(), 2.0, 1e-9);
  EXPECT_NEAR(mx.z(), 2.0, 1e-9);
}

TEST(BoxBoolean, IntersectDisjointFailsWithDiagnostics) {
  Model model;
  Body* a = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "A"});
  Body* b = make_box(model, BoxSpec{.min = {2, 0, 0}, .max = {3, 1, 1}, .name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Intersect, model, *a, *b, {});
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.body, nullptr);
  EXPECT_FALSE(result.diagnostics.empty());
}

TEST(BoxBoolean, SubtractPartialYieldsLShape) {
  Model model;
  Body* a = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 2, 1}, .name = "A"});
  Body* b = make_box(model, BoxSpec{.min = {1, 1, 0}, .max = {2, 2, 1}, .name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *a, *b, {});
  ASSERT_TRUE(result.ok()) << result.diagnostics;
  EXPECT_TRUE(validate_body(*result.body).ok());
  // Volume = 4 - 1 = 3; AABB still 2x2x1
  auto [mn, mx] = body_aabb(*result.body);
  EXPECT_NEAR(aabb_volume(mn, mx), 4.0, 1e-9);
  // Result should have more than 6 faces (L-shape)
  ASSERT_FALSE(result.body->shells.empty());
  EXPECT_GT(result.body->shells[0]->faces.size(), 6u);
}

TEST(BoxBoolean, SubtractContainedFailsEmpty) {
  Model model;
  Body* a = make_box(model, BoxSpec{.min = {1, 1, 1}, .max = {2, 2, 2}, .name = "A"});
  Body* b = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {3, 3, 3}, .name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *a, *b, {});
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.diagnostics.empty());
}

TEST(BoxBoolean, SubtractNoOverlapKeepsTarget) {
  Model model;
  Body* a = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "A"});
  Body* b = make_box(model, BoxSpec{.min = {2, 0, 0}, .max = {3, 1, 1}, .name = "B"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *a, *b, {});
  ASSERT_TRUE(result.ok()) << result.diagnostics;
  EXPECT_TRUE(validate_body(*result.body).ok());
  auto [mn, mx] = body_aabb(*result.body);
  EXPECT_NEAR(aabb_volume(mn, mx), 1.0, 1e-9);
}

TEST(BoxBoolean, RejectsSphereBoxPair) {
  Model model;
  Body* box = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 2, 2}});
  Body* sphere =
      make_sphere(model, SphereSpec{.center = {1, 1, 1}, .radius = 0.5});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *box, *sphere, {});
  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.diagnostics.find("unsupported"), std::string::npos);
}

TEST(BoxBoolean, PartAddBooleanUsesDefaultEvaluator) {
  auto doc = Document::create("box_bool_part");
  Part& part = doc->add_part("Main");
  Body* a = part.add_box(BoxSpec{.min = {0, 0, 0}, .max = {2, 1, 1}, .name = "A"});
  Body* b = part.add_box(BoxSpec{.min = {1, 0, 0}, .max = {3, 1, 1}, .name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto tid = part.features().find_by_body(a->guid)->id();
  const auto tool = part.features().find_by_body(b->guid)->id();
  Body* out = part.add_boolean(boolean::BooleanOp::Union, tid, tool, "Fuse");
  ASSERT_NE(out, nullptr);
  EXPECT_TRUE(validate_body(*out).ok());
  EXPECT_EQ(part.model().bodies().size(), 1u);
}

}  // namespace
}  // namespace brep
