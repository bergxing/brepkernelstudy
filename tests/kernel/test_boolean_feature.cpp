#include "api/core.hpp"
#include "api/modeling.hpp"
#include "api/persistence.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/feat/boolean_feature.hpp"
#include "brep/io/bks_cache.hpp"

#include <gtest/gtest.h>

#include <filesystem>
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
  part.set_boolean_evaluator(boolean::make_stub_boolean_evaluator());

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

TEST(BooleanFeature, XlRoundtripPersistsBooleanAndSuppress) {
  namespace fs = std::filesystem;

  struct FactoryGuard {
    FactoryGuard() {
      boolean::set_boolean_evaluator_factory(
          [] { return std::make_shared<FakeUnionEvaluator>(); });
    }
    ~FactoryGuard() { boolean::set_boolean_evaluator_factory({}); }
  } factory_guard;

  auto doc = Document::create("bool_xl");
  Part& part = doc->add_part("Main");

  Body* a = part.add_box(BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "A"});
  Body* b =
      part.add_box(BoxSpec{.min = {0.5, 0, 0}, .max = {1.5, 1, 1}, .name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto target_id = part.features().find_by_body(a->guid)->id();
  const auto tool_id = part.features().find_by_body(b->guid)->id();

  Body* result =
      part.add_boolean(boolean::BooleanOp::Subtract, target_id, tool_id, "Cut");
  ASSERT_NE(result, nullptr);
  const Guid result_guid = result->guid;
  const auto bool_id = part.features().find_by_body(result_guid)->id();

  const fs::path path =
      fs::temp_directory_path() / "brep_test_boolean_feature_roundtrip.xl";
  auto saved = io::save_xl(*doc, path);
  ASSERT_TRUE(saved.ok) << saved.error;

  auto loaded = io::load_xl(path);
  ASSERT_TRUE(loaded.ok()) << loaded.error;

  Part* p2 = loaded.document->main_part();
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(p2->model().bodies().size(), 1u);
  EXPECT_NE(p2->find_body(result_guid), nullptr);

  auto* target2 = p2->features().find(target_id);
  auto* tool2 = p2->features().find(tool_id);
  auto* bool2 = p2->features().find(bool_id);
  ASSERT_NE(target2, nullptr);
  ASSERT_NE(tool2, nullptr);
  ASSERT_NE(bool2, nullptr);
  EXPECT_TRUE(target2->suppressed());
  EXPECT_TRUE(tool2->suppressed());
  EXPECT_FALSE(bool2->suppressed());
  EXPECT_EQ(bool2->type_name(), "Boolean");
  EXPECT_EQ(bool2->body_guid(), result_guid);

  const auto& bf = static_cast<const feat::BooleanFeature&>(*bool2);
  EXPECT_EQ(bf.op(), boolean::BooleanOp::Subtract);
  EXPECT_EQ(bf.target_feature_id(), target_id);
  EXPECT_EQ(bf.tool_feature_id(), tool_id);

  std::error_code ec;
  fs::remove(path, ec);
  fs::remove(io::bks_cache_path_for(path), ec);
}

}  // namespace
}  // namespace brep
