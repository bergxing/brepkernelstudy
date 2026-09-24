#include "api/Core.h"
#include "api/Modeling.h"
#include "api/Persistence.h"

#include "brep/bool/Evaluator.h"
#include "brep/feat/NurbsCurveFeature.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <variant>

namespace brep
{
namespace
{

NurbsCurveSpec SampleFiveCvSpec()
{
    return NurbsCurveSpec{
        .Cvs = {{0, 0, 0}, {1, 0, 0}, {2, 1, 0}, {3, 1, 0}, {4, 0, 0}},
        .Tolerance = 1e-7,
        .Name = "nurbs",
    };
}

TEST(NurbsFeature, AddNurbsCurveCreatesWireBody)
{
    auto doc = Document::Create("nurbs_add",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    Body* body = part.AddNurbsCurve(SampleFiveCvSpec());
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(part.Model().Bodies().size(), 1u);
    EXPECT_EQ(body->Type, BodyType::Wire);
    ASSERT_EQ(body->WireEdges.size(), 1u);

    auto* feature = part.Features().FindByBody(body->Guid);
    ASSERT_NE(feature, nullptr);
    EXPECT_EQ(feature->TypeName(), "NurbsCurve");

    auto prim = feature->ToPrimitiveSpec(part.Parameters());
    ASSERT_TRUE(prim.has_value());
    const auto* nurbs = std::get_if<NurbsCurveSpec>(&*prim);
    ASSERT_NE(nurbs, nullptr);
    EXPECT_EQ(nurbs->Cvs.size(), 5u);
    EXPECT_EQ(nurbs->Knots.size(), 9u);
}

TEST(NurbsFeature, UndoRedoRestoresBody)
{
    auto doc = Document::Create("nurbs_hist",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{1, 0, 0}, Point3d{2, 1, 0},
                Point3d{3, 0, 0}};
    Body* body = part.AddNurbsCurve(spec);
    ASSERT_NE(body, nullptr);
    const Guid id = body->Guid;
    auto* feature = part.Features().FindByBody(id);
    ASSERT_NE(feature, nullptr);

    feat::FeatureTransaction tx;
    tx.Kind = feat::TxKind::AppendFeature;
    tx.Feature = feature->Id();
    tx.FeatureType = "NurbsCurve";
    tx.Nurbs = static_cast<feat::NurbsCurveFeature*>(feature)->ToSpec();
    part.FeatureHistory().Record(std::move(tx));

    ASSERT_TRUE(part.FeatureHistory().Undo(part));
    EXPECT_EQ(part.FindBody(id), nullptr);

    ASSERT_TRUE(part.FeatureHistory().Redo(part));
    EXPECT_FALSE(part.Model().Bodies().empty());
    Body* restored = part.Model().Bodies().front().get();
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->Type, BodyType::Wire);
}

TEST(NurbsFeature, TransformBodyMovesCvsKeepsKnotsAndWeights)
{
    auto doc = Document::Create("nurbs_xf",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{1, 0, 0}, Point3d{2, 1, 0},
                Point3d{3, 1, 0}, Point3d{4, 0, 0}};
    spec.Weights = {1, 1, 2, 1, 1};
    Body* body = part.AddNurbsCurve(spec);
    ASSERT_NE(body, nullptr);
    const Guid guid = body->Guid;

    RigidTransform t{.Translation = Point3d{2, 3, 0}};
    ASSERT_TRUE(part.TransformBody(guid, t));

    auto* feature = part.Features().FindByBody(guid);
    ASSERT_NE(feature, nullptr);
    auto prim = feature->ToPrimitiveSpec(part.Parameters());
    ASSERT_TRUE(prim.has_value());
    const auto* nurbs = std::get_if<NurbsCurveSpec>(&*prim);
    ASSERT_NE(nurbs, nullptr);
    ASSERT_EQ(nurbs->Cvs.size(), 5u);
    EXPECT_NEAR(nurbs->Cvs[0].x(), 2.0, 1e-12);
    EXPECT_NEAR(nurbs->Cvs[0].y(), 3.0, 1e-12);
    EXPECT_NEAR(nurbs->Cvs[4].x(), 6.0, 1e-12);
    ASSERT_EQ(nurbs->Weights.size(), 5u);
    EXPECT_NEAR(nurbs->Weights[2], 2.0, 1e-12);
    ASSERT_EQ(nurbs->Knots.size(), 9u);
    EXPECT_DOUBLE_EQ(nurbs->Knots[4], 0.5);
}

TEST(NurbsFeature, XlRoundTripKeepsKnotsAndWeights)
{
    namespace fs = std::filesystem;
    auto doc =
        Document::Create("nurbs_xl", boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{1, 0, 0}, Point3d{2, 1, 0},
                Point3d{3, 1, 0}, Point3d{4, 0, 0}};
    spec.Weights = {1, 1, 2, 1, 1};
    Body* body = part.AddNurbsCurve(spec);
    ASSERT_NE(body, nullptr);
    const Guid bodyGuid = body->Guid;

    const fs::path path =
        fs::temp_directory_path() / "brep_test_nurbs_feature_roundtrip.xl";
    auto saved = io::SaveXl(*doc, path);
    ASSERT_TRUE(saved.Ok) << saved.Error;

    auto loaded = io::LoadXl(path);
    ASSERT_TRUE(loaded.Ok()) << loaded.Error;
    Part* p2 = loaded.document->MainPart();
    ASSERT_NE(p2, nullptr);

    auto* feat = p2->Features().FindByBody(bodyGuid);
    ASSERT_NE(feat, nullptr);
    EXPECT_EQ(feat->TypeName(), "NurbsCurve");
    const auto prim = feat->ToPrimitiveSpec(p2->Parameters());
    ASSERT_TRUE(prim.has_value());
    const auto* nurbs = std::get_if<NurbsCurveSpec>(&*prim);
    ASSERT_NE(nurbs, nullptr);
    ASSERT_EQ(nurbs->Cvs.size(), 5u);
    ASSERT_EQ(nurbs->Weights.size(), 5u);
    EXPECT_NEAR(nurbs->Weights[2], 2.0, 1e-12);
    ASSERT_EQ(nurbs->Knots.size(), 9u);
    EXPECT_DOUBLE_EQ(nurbs->Knots[4], 0.5);

    std::error_code ec;
    fs::remove(path, ec);
    fs::remove(io::BksCachePathFor(path), ec);
}

}  // namespace
}  // namespace brep
