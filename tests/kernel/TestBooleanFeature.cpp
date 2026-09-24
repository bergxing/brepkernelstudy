#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "api/Persistence.h"

#include "brep/bool/Boolean.h"
#include "brep/bool/Evaluator.h"
#include "brep/feat/BooleanFeature.h"
#include "brep/io/BksCache.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

namespace brep
{
namespace
{

class FakeUnionEvaluator final : public boolean::IBooleanEvaluator
{
 public:
  boolean::BooleanResult Evaluate(boolean::BooleanOp op, Model& model, const Body& a,
                                  const Body& b,
                                  const boolean::BooleanContext&) override
  {
    m_lastOp = op;
    boolean::BooleanResult result;
    result.Mode = boolean::BooleanEvalMode::General;
    BoxSpec spec;
    spec.Min = Point3d{0, 0, 0};
    spec.Max = Point3d{1, 1, 1};
    spec.Name = std::string("bool_") + a.Name + "_" + b.Name;
    result.OutputBody = MakeBox(model, spec);
    return result;
  }

  boolean::BooleanOp m_lastOp{boolean::BooleanOp::Union};
};

std::shared_ptr<boolean::IBooleanEvaluator> MakeFakeUnionEvaluator()
{
  return std::make_shared<FakeUnionEvaluator>();
}

TEST(BooleanFeature, SuppressesOperandsResultVisible)
{
  auto doc = Document::Create("bool_feat", MakeFakeUnionEvaluator());
  Part& part = doc->AddPart("Main");

  Body* a = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b =
      part.AddBox(BoxSpec{.Min = {0.5, 0.5, 0.5}, .Max = {1.5, 1.5, 1.5},
                           .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const Guid aGuid = a->Guid;
  const Guid bGuid = b->Guid;

  auto* fa = part.Features().FindByBody(aGuid);
  auto* fb = part.Features().FindByBody(bGuid);
  ASSERT_NE(fa, nullptr);
  ASSERT_NE(fb, nullptr);

  Body* result =
      part.AddBoolean(boolean::BooleanOp::Union, fa->Id(), fb->Id(), "Fuse");
  ASSERT_NE(result, nullptr);

  EXPECT_EQ(part.Model().Bodies().size(), 1u);
  EXPECT_EQ(part.FindBody(aGuid), nullptr);
  EXPECT_EQ(part.FindBody(bGuid), nullptr);
  EXPECT_NE(part.FindBody(result->Guid), nullptr);

  EXPECT_TRUE(part.Features().Find(fa->Id())->Suppressed());
  EXPECT_TRUE(part.Features().Find(fb->Id())->Suppressed());
  EXPECT_FALSE(part.Features().FindByBody(result->Guid)->Suppressed());
  EXPECT_EQ(part.Features().FindByBody(result->Guid)->TypeName(), "Boolean");
}

TEST(BooleanFeature, UndoRedoRestoresOperands)
{
  auto doc = Document::Create("bool_undo", MakeFakeUnionEvaluator());
  Part& part = doc->AddPart("Main");

  Body* a = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b = part.AddBox(BoxSpec{.Min = {1, 0, 0}, .Max = {2, 1, 1}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto target = part.Features().FindByBody(a->Guid)->Id();
  const auto tool = part.Features().FindByBody(b->Guid)->Id();

  Body* result =
      part.AddBoolean(boolean::BooleanOp::Intersect, target, tool, "Common");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
  ASSERT_TRUE(part.FeatureHistory().CanUndo());

  ASSERT_TRUE(part.FeatureHistory().Undo(part));
  EXPECT_FALSE(part.Features().Find(target)->Suppressed());
  EXPECT_FALSE(part.Features().Find(tool)->Suppressed());
  EXPECT_EQ(part.Model().Bodies().size(), 2u);
  EXPECT_EQ(part.Features().Find(target)->TypeName(), "Box");

  ASSERT_TRUE(part.FeatureHistory().CanRedo());
  ASSERT_TRUE(part.FeatureHistory().Redo(part));
  EXPECT_TRUE(part.Features().Find(target)->Suppressed());
  EXPECT_TRUE(part.Features().Find(tool)->Suppressed());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
  auto* boolF = part.Features().Features().back().get();
  ASSERT_NE(boolF, nullptr);
  EXPECT_EQ(boolF->TypeName(), "Boolean");
  EXPECT_NE(part.FindBody(boolF->BodyGuid()), nullptr);
}

TEST(BooleanFeature, StubEvaluatorFailsWithoutSuppress)
{
  auto stub = boolean::MakeStubBooleanEvaluator();
  auto doc = Document::Create(
      "bool_stub",
      std::shared_ptr<boolean::IBooleanEvaluator>(stub.release()));
  Part& part = doc->AddPart("Main");

  Body* a = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto target = part.Features().FindByBody(a->Guid)->Id();
  const auto tool = part.Features().FindByBody(b->Guid)->Id();

  Body* result = part.AddBoolean(boolean::BooleanOp::Union, target, tool);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(part.Model().Bodies().size(), 2u);
  EXPECT_FALSE(part.Features().Find(target)->Suppressed());
  EXPECT_FALSE(part.Features().Find(tool)->Suppressed());
}

TEST(BooleanFeature, XlRoundtripPersistsBooleanAndSuppress)
{
  namespace fs = std::filesystem;

  auto doc = Document::Create("bool_xl", MakeFakeUnionEvaluator());
  Part& part = doc->AddPart("Main");

  Body* a = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b =
      part.AddBox(BoxSpec{.Min = {0.5, 0, 0}, .Max = {1.5, 1, 1}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto targetId = part.Features().FindByBody(a->Guid)->Id();
  const auto toolId = part.Features().FindByBody(b->Guid)->Id();

  Body* result =
      part.AddBoolean(boolean::BooleanOp::Subtract, targetId, toolId, "Cut");
  ASSERT_NE(result, nullptr);
  const Guid resultGuid = result->Guid;
  const auto boolId = part.Features().FindByBody(resultGuid)->Id();

  const fs::path path =
      fs::temp_directory_path() / "brep_test_boolean_feature_roundtrip.xl";
  auto saved = io::SaveXl(*doc, path);
  ASSERT_TRUE(saved.Ok) << saved.Error;

  auto loaded = io::LoadXl(path);
  ASSERT_TRUE(loaded.Ok()) << loaded.Error;

  Part* p2 = loaded.document->MainPart();
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(p2->Model().Bodies().size(), 1u);
  EXPECT_NE(p2->FindBody(resultGuid), nullptr);

  auto* target2 = p2->Features().Find(targetId);
  auto* tool2 = p2->Features().Find(toolId);
  auto* bool2 = p2->Features().Find(boolId);
  ASSERT_NE(target2, nullptr);
  ASSERT_NE(tool2, nullptr);
  ASSERT_NE(bool2, nullptr);
  EXPECT_TRUE(target2->Suppressed());
  EXPECT_TRUE(tool2->Suppressed());
  EXPECT_FALSE(bool2->Suppressed());
  EXPECT_EQ(bool2->TypeName(), "Boolean");
  EXPECT_EQ(bool2->BodyGuid(), resultGuid);

  const auto& bf = static_cast<const feat::BooleanFeature&>(*bool2);
  EXPECT_EQ(bf.Op(), boolean::BooleanOp::Subtract);
  EXPECT_EQ(bf.TargetFeatureId(), targetId);
  EXPECT_EQ(bf.ToolFeatureId(), toolId);

  std::error_code ec;
  fs::remove(path, ec);
  fs::remove(io::BksCachePathFor(path), ec);
}

}  // namespace
}  // namespace brep
