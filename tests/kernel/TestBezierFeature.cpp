#include "api/Core.h"
#include "api/Modeling.h"
#include "api/Persistence.h"

#include "brep/bool/Evaluator.h"
#include "brep/feat/BezierCurveFeature.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <variant>

namespace brep
{
namespace
{

BezierSpec SampleSpec()
{
    return BezierSpec{
        .Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
        .Tolerance = 1e-7,
        .Name = "bezier",
    };
}

TEST(BezierFeature, AddBezierCreatesWireBody)
{
    auto doc = Document::Create("bez_add",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    Body* body = part.AddBezier(SampleSpec());
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(part.Model().Bodies().size(), 1u);
    EXPECT_EQ(body->Type, BodyType::Wire);
    ASSERT_EQ(body->WireEdges.size(), 1u);

    auto* feature = part.Features().FindByBody(body->Guid);
    ASSERT_NE(feature, nullptr);
    EXPECT_EQ(feature->TypeName(), "Bezier");
}

TEST(BezierFeature, ToPrimitiveSpecHitsBezierSpec)
{
    auto doc = Document::Create("bez_spec",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    const BezierSpec input = SampleSpec();
    Body* body = part.AddBezier(input);
    ASSERT_NE(body, nullptr);
    auto* feature = part.Features().FindByBody(body->Guid);
    ASSERT_NE(feature, nullptr);

    auto prim = feature->ToPrimitiveSpec(part.Parameters());
    ASSERT_TRUE(prim.has_value());
    const auto* bez = std::get_if<BezierSpec>(&*prim);
    ASSERT_NE(bez, nullptr);
    EXPECT_NEAR((bez->P0() - input.P0()).norm(), 0.0, 1e-12);
    EXPECT_NEAR((bez->P1() - input.P1()).norm(), 0.0, 1e-12);
    EXPECT_NEAR((bez->P2() - input.P2()).norm(), 0.0, 1e-12);
    EXPECT_NEAR((bez->P3() - input.P3()).norm(), 0.0, 1e-12);
    EXPECT_NEAR(bez->Tolerance, input.Tolerance, 1e-15);
}

TEST(BezierFeature, HistoryUndoRedo)
{
    auto doc = Document::Create("bez_hist",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    const BezierSpec spec = SampleSpec();
    Body* body = part.AddBezier(spec);
    ASSERT_NE(body, nullptr);
    auto* feature = part.Features().FindByBody(body->Guid);
    ASSERT_NE(feature, nullptr);

    feat::FeatureTransaction tx;
    tx.Kind = feat::TxKind::AppendFeature;
    tx.Feature = feature->Id();
    tx.FeatureType = "Bezier";
    tx.Bezier = spec;
    part.FeatureHistory().Record(std::move(tx));

    ASSERT_TRUE(part.FeatureHistory().CanUndo());
    ASSERT_TRUE(part.FeatureHistory().Undo(part));
    EXPECT_TRUE(part.Model().Bodies().empty());
    EXPECT_TRUE(part.Features().Features().empty());

    ASSERT_TRUE(part.FeatureHistory().CanRedo());
    ASSERT_TRUE(part.FeatureHistory().Redo(part));
    ASSERT_EQ(part.Model().Bodies().size(), 1u);
    Body* restored = part.Model().Bodies().front().get();
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->Type, BodyType::Wire);
    EXPECT_EQ(restored->WireEdges.size(), 1u);
}

TEST(BezierFeature, XlRoundtrip)
{
    namespace fs = std::filesystem;
    auto doc =
        Document::Create("bez_xl", boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    const BezierSpec spec = SampleSpec();
    Body* body = part.AddBezier(spec);
    ASSERT_NE(body, nullptr);
    const Guid bodyGuid = body->Guid;

    const fs::path path =
        fs::temp_directory_path() / "brep_test_bezier_feature_roundtrip.xl";
    auto saved = io::SaveXl(*doc, path);
    ASSERT_TRUE(saved.Ok) << saved.Error;

    auto loaded = io::LoadXl(path);
    ASSERT_TRUE(loaded.Ok()) << loaded.Error;
    Part* p2 = loaded.document->MainPart();
    ASSERT_NE(p2, nullptr);
    Body* body2 = p2->FindBody(bodyGuid);
    ASSERT_NE(body2, nullptr);
    EXPECT_EQ(body2->Type, BodyType::Wire);
    ASSERT_EQ(body2->WireEdges.size(), 1u);

    auto* feat = p2->Features().FindByBody(bodyGuid);
    ASSERT_NE(feat, nullptr);
    EXPECT_EQ(feat->TypeName(), "Bezier");
    auto prim = feat->ToPrimitiveSpec(p2->Parameters());
    ASSERT_TRUE(prim.has_value());
    const auto* bez = std::get_if<BezierSpec>(&*prim);
    ASSERT_NE(bez, nullptr);
    EXPECT_NEAR((bez->P0() - spec.P0()).norm(), 0.0, 1e-12);
    EXPECT_NEAR((bez->P1() - spec.P1()).norm(), 0.0, 1e-12);
    EXPECT_NEAR((bez->P2() - spec.P2()).norm(), 0.0, 1e-12);
    EXPECT_NEAR((bez->P3() - spec.P3()).norm(), 0.0, 1e-12);
    EXPECT_NEAR(bez->Tolerance, spec.Tolerance, 1e-15);

    std::error_code ec;
    fs::remove(path, ec);
    fs::remove(io::BksCachePathFor(path), ec);
}

TEST(BezierFeature, XlRoundtripMultiAndElevated)
{
    namespace fs = std::filesystem;
    auto doc =
        Document::Create("bez_xl2", boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    BezierSpec multi{
        .Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0},
                {1.5, -0.5, 0}, {2, 0, 0}, {2, 1, 0}},
        .Degree = 3,
        .SegmentCount = 2,
        .Corner = {1},
        .Tolerance = 1e-7,
        .Name = "poly",
    };
    Body* body = part.AddBezier(multi);
    ASSERT_NE(body, nullptr);
    const Guid multiGuid = body->Guid;

    BezierSpec elevated = ElevateBezierDegree(SampleSpec());
    elevated.Name = "hi";
    Body* body2 = part.AddBezier(elevated);
    ASSERT_NE(body2, nullptr);
    const Guid elevGuid = body2->Guid;

    const fs::path path =
        fs::temp_directory_path() / "brep_test_bezier_feature_ex.xl";
    ASSERT_TRUE(io::SaveXl(*doc, path).Ok);

    auto loaded = io::LoadXl(path);
    ASSERT_TRUE(loaded.Ok()) << loaded.Error;
    Part* p2 = loaded.document->MainPart();
    ASSERT_NE(p2, nullptr);

    auto* fMulti = p2->Features().FindByBody(multiGuid);
    ASSERT_NE(fMulti, nullptr);
    auto primM = fMulti->ToPrimitiveSpec(p2->Parameters());
    ASSERT_TRUE(primM);
    const auto* bm = std::get_if<BezierSpec>(&*primM);
    ASSERT_NE(bm, nullptr);
    EXPECT_EQ(bm->SegmentCount, 2);
    EXPECT_EQ(bm->Cvs.size(), 7u);
    ASSERT_EQ(bm->Corner.size(), 1u);
    EXPECT_EQ(bm->Corner[0], 1);

    auto* fElev = p2->Features().FindByBody(elevGuid);
    ASSERT_NE(fElev, nullptr);
    auto primE = fElev->ToPrimitiveSpec(p2->Parameters());
    ASSERT_TRUE(primE);
    const auto* be = std::get_if<BezierSpec>(&*primE);
    ASSERT_NE(be, nullptr);
    EXPECT_EQ(be->Degree, elevated.Degree);
    EXPECT_EQ(be->Cvs.size(), elevated.Cvs.size());
    EXPECT_EQ(be->SegmentCount, 1);

    std::error_code ec;
    fs::remove(path, ec);
    fs::remove(io::BksCachePathFor(path), ec);
}

TEST(BezierFeature, XlRoundtripRationalWeights)
{
    namespace fs = std::filesystem;
    auto doc =
        Document::Create("bez_w", boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");
    BezierSpec spec{
        .Cvs = {{1, 0, 0}, {1, 1, 0}, {0, 1, 0}},
        .Weights = {1.0, 0.7071067811865476, 1.0},
        .Degree = 2,
        .SegmentCount = 1,
        .Tolerance = 1e-7,
        .Name = "arc",
    };
    Body* body = part.AddBezier(spec);
    ASSERT_NE(body, nullptr);
    const Guid bodyGuid = body->Guid;

    const fs::path path =
        fs::temp_directory_path() / "brep_test_bezier_feature_w.xl";
    ASSERT_TRUE(io::SaveXl(*doc, path).Ok);

    auto loaded = io::LoadXl(path);
    ASSERT_TRUE(loaded.Ok()) << loaded.Error;
    Part* p2 = loaded.document->MainPart();
    ASSERT_NE(p2, nullptr);
    auto* feat = p2->Features().FindByBody(bodyGuid);
    ASSERT_NE(feat, nullptr);
    auto prim = feat->ToPrimitiveSpec(p2->Parameters());
    ASSERT_TRUE(prim);
    const auto* bez = std::get_if<BezierSpec>(&*prim);
    ASSERT_NE(bez, nullptr);
    ASSERT_EQ(bez->Weights.size(), 3u);
    EXPECT_NEAR(bez->Weights[1], spec.Weights[1], 1e-12);

    std::error_code ec;
    fs::remove(path, ec);
    fs::remove(io::BksCachePathFor(path), ec);
}

TEST(BezierFeature, AddPrimitiveVisitsBezier)
{
    auto doc = Document::Create("bez_prim",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");
    Body* body = part.AddPrimitive(SampleSpec());
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->Type, BodyType::Wire);
}

}  // namespace
}  // namespace brep
