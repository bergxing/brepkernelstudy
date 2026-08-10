#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/feat/boolean_feature.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace brep {
namespace {

/// Test double: builds a unit box as a stand-in boolean result in `model`.
class FakeUnionEvaluator final : public boolean::IBooleanEvaluator {
 public:
  boolean::BooleanResult evaluate(boolean::BooleanOp op, Model& model,
                                  const Body& a, const Body& b,
                                  const boolean::BooleanContext&) override {
    last_op = op;
    boolean::BooleanResult result;
    result.mode = boolean::BooleanEvalMode::AnalyticPair;
    BoxSpec spec;
    spec.min = Point3d{0, 0, 0};
    spec.max = Point3d{1, 1, 1};
    spec.name = std::string("bool_") + a.name + "_" + b.name;
    result.body = make_box(model, spec);
    return result;
  }

  boolean::BooleanOp last_op{boolean::BooleanOp::Union};
};

TEST(BooleanFeature, SuppressesOperandsResultVisible) {
  auto doc = Document::create("bool_feat");
  Part& part = doc->add_part("Main");
  part.set_boolean_evaluator(std::make_shared<FakeUnionEvaluator>());

  Body* a = part.add_box(BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "A"});
  Body* b =
      part.add_box(BoxSpec{.min = {0.5, 0.5, 0.5}, .max = {1.5, 1.5, 1.5},
                           .name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const Guid a_guid = a->guid;
  const Guid b_guid = b->guid;

  auto* fa = part.features().find_by_body(a_guid);
  auto* fb = part.features().find_by_body(b_guid);
  ASSERT_NE(fa, nullptr);
  ASSERT_NE(fb, nullptr);

  Body* result =
      part.add_boolean(boolean::BooleanOp::Union, fa->id(), fb->id(), "Fuse");
  ASSERT_NE(result, nullptr);

  EXPECT_EQ(part.model().bodies().size(), 1u);
  EXPECT_EQ(part.find_body(a_guid), nullptr);
  EXPECT_EQ(part.find_body(b_guid), nullptr);
  EXPECT_NE(part.find_body(result->guid), nullptr);

  EXPECT_TRUE(part.features().find(fa->id())->suppressed());
  EXPECT_TRUE(part.features().find(fb->id())->suppressed());
  EXPECT_FALSE(part.features().find_by_body(result->guid)->suppressed());
  EXPECT_EQ(part.features().find_by_body(result->guid)->type_name(), "Boolean");
}

TEST(BooleanFeature, UndoRedoRestoresOperands) {
  auto doc = Document::create("bool_undo");
  Part& part = doc->add_part("Main");
  part.set_boolean_evaluator(std::make_shared<FakeUnionEvaluator>());

  Body* a = part.add_box(BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "A"});
  Body* b = part.add_box(BoxSpec{.min = {1, 0, 0}, .max = {2, 1, 1}, .name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto target = part.features().find_by_body(a->guid)->id();
  const auto tool = part.features().find_by_body(b->guid)->id();

  Body* result =
      part.add_boolean(boolean::BooleanOp::Intersect, target, tool, "Common");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(part.model().bodies().size(), 1u);
  ASSERT_TRUE(part.feature_history().can_undo());

  ASSERT_TRUE(part.feature_history().undo(part));
  EXPECT_FALSE(part.features().find(target)->suppressed());
  EXPECT_FALSE(part.features().find(tool)->suppressed());
  EXPECT_EQ(part.model().bodies().size(), 2u);
  EXPECT_EQ(part.features().find(target)->type_name(), "Box");

  ASSERT_TRUE(part.feature_history().can_redo());
  ASSERT_TRUE(part.feature_history().redo(part));
  EXPECT_TRUE(part.features().find(target)->suppressed());
  EXPECT_TRUE(part.features().find(tool)->suppressed());
  EXPECT_EQ(part.model().bodies().size(), 1u);
  auto* bool_f = part.features().features().back().get();
  ASSERT_NE(bool_f, nullptr);
  EXPECT_EQ(bool_f->type_name(), "Boolean");
  EXPECT_NE(part.find_body(bool_f->body_guid()), nullptr);
}

TEST(BooleanFeature, StubEvaluatorFailsWithoutSuppress) {
  auto doc = Document::create("bool_stub");
  Part& part = doc->add_part("Main");
  // default stub evaluator

  Body* a = part.add_box(BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "A"});
  Body* b = part.add_box(BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto target = part.features().find_by_body(a->guid)->id();
  const auto tool = part.features().find_by_body(b->guid)->id();

  Body* result = part.add_boolean(boolean::BooleanOp::Union, target, tool);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(part.model().bodies().size(), 2u);
  EXPECT_FALSE(part.features().find(target)->suppressed());
  EXPECT_FALSE(part.features().find(tool)->suppressed());
}

}  // namespace
}  // namespace brep
