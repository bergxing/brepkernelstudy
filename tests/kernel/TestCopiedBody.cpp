#include "api/Core.h"
#include "api/Modeling.h"
#include "api/Persistence.h"

#include "brep/bool/Evaluator.h"
#include "brep/feat/CopiedBodyFeature.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <variant>

namespace brep
{
namespace
{

Point3d FirstVertex(const Body& body)
{
    for (Shell* shell : body.Shells)
    {
        if (!shell)
        {
            continue;
        }
        for (Face* face : shell->Faces)
        {
            if (!face)
            {
                continue;
            }
            for (Loop* loop : face->Loops)
            {
                if (!loop)
                {
                    continue;
                }
                Point3d found{};
                bool ok = false;
                loop->ForEachCoedge(
                    [&](CoEdge& c)
                    {
                        if (ok)
                        {
                            return;
                        }
                        Vertex* v = c.From();
                        if (v && v->Point)
                        {
                            found = v->Position();
                            ok = true;
                        }
                    });
                if (ok)
                {
                    return found;
                }
            }
        }
    }
    return {};
}

void VertexYRange(const Body& body, double& minY, double& maxY)
{
    minY = 1e9;
    maxY = -1e9;
    bool any = false;
    for (Shell* shell : body.Shells)
    {
        if (!shell)
        {
            continue;
        }
        for (Face* face : shell->Faces)
        {
            if (!face)
            {
                continue;
            }
            for (Loop* loop : face->Loops)
            {
                if (!loop)
                {
                    continue;
                }
                loop->ForEachCoedge(
                    [&](CoEdge& c)
                    {
                        Vertex* v = c.From();
                        if (!v || !v->Point)
                        {
                            return;
                        }
                        const double y = v->Position().y();
                        minY = std::min(minY, y);
                        maxY = std::max(maxY, y);
                        any = true;
                    });
            }
        }
    }
    if (!any)
    {
        minY = 0.0;
        maxY = 0.0;
    }
}

TEST(CopiedBody, DuplicateUnionTranslatesIndependentBody)
{
    auto doc = Document::Create("copy_union",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    Body* a = part.AddBox(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
    Body* b = part.AddBox(
        BoxSpec{.Min = {0.5, 0, 0}, .Max = {1.5, 1, 1}, .Name = "B"});
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const auto target = part.Features().FindByBody(a->Guid)->Id();
    const auto tool = part.Features().FindByBody(b->Guid)->Id();
    Body* fused =
        part.AddBoolean(boolean::BooleanOp::Union, target, tool, "Fuse");
    ASSERT_NE(fused, nullptr);
    const Guid srcGuid = fused->Guid;
    RigidTransform t{.Translation = Point3d{0, 4, 0}};
    Body* copy = part.DuplicateBody(srcGuid, t, "FuseCopy");
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy->Guid, srcGuid);
    EXPECT_EQ(part.Model().Bodies().size(), 2u);
    EXPECT_TRUE(ValidateBody(*copy).Ok());
    double srcMin = 0.0;
    double srcMax = 0.0;
    double copyMin = 0.0;
    double copyMax = 0.0;
    VertexYRange(*fused, srcMin, srcMax);
    VertexYRange(*copy, copyMin, copyMax);
    EXPECT_NEAR(copyMin, srcMin + 4.0, 1e-6);
    EXPECT_NEAR(copyMax, srcMax + 4.0, 1e-6);

    auto* copyFeat = part.Features().FindByBody(copy->Guid);
    ASSERT_NE(copyFeat, nullptr);
    EXPECT_EQ(copyFeat->TypeName(), "CopiedBody");
    EXPECT_FALSE(copyFeat->ToPrimitiveSpec(part.Parameters()).has_value());
}

TEST(CopiedBody, RegenFollowsSourceThenFailsIfSourceRemoved)
{
    auto doc = Document::Create("copy_derived",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    Body* a = part.AddBox(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
    Body* b = part.AddBox(
        BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const auto target = part.Features().FindByBody(a->Guid)->Id();
    const auto tool = part.Features().FindByBody(b->Guid)->Id();
    Body* fused =
        part.AddBoolean(boolean::BooleanOp::Union, target, tool, "Fuse");
    ASSERT_NE(fused, nullptr);
    const Guid srcGuid = fused->Guid;
    const auto boolId = part.Features().FindByBody(srcGuid)->Id();

    RigidTransform t{.Translation = Point3d{0, 3, 0}};
    Body* copy = part.DuplicateBody(srcGuid, t);
    ASSERT_NE(copy, nullptr);
    const Guid copyGuid = copy->Guid;

    ASSERT_TRUE(part.EditFeatureParams(target, {{"Length", 1.2}}));
    Body* srcAfter = part.FindBody(srcGuid);
    Body* copyAfter = part.FindBody(copyGuid);
    ASSERT_NE(srcAfter, nullptr);
    ASSERT_NE(copyAfter, nullptr);
    double srcMin = 0.0;
    double srcMax = 0.0;
    double copyMin = 0.0;
    double copyMax = 0.0;
    VertexYRange(*srcAfter, srcMin, srcMax);
    VertexYRange(*copyAfter, copyMin, copyMax);
    EXPECT_NEAR(copyMin, srcMin + 3.0, 1e-6);
    EXPECT_NEAR(copyMax, srcMax + 3.0, 1e-6);

    ASSERT_TRUE(part.RemoveFeature(boolId));
    auto* copyFeat = part.Features().FindByBody(copyGuid);
    if (copyFeat)
    {
        EXPECT_EQ(copyFeat->Status(), feat::FeatureStatus::Failed);
    }
}

TEST(CopiedBody, XlRoundtrip)
{
    namespace fs = std::filesystem;
    auto doc =
        Document::Create("copy_xl", boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");

    Body* a = part.AddBox(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
    Body* b = part.AddBox(
        BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
    const auto target = part.Features().FindByBody(a->Guid)->Id();
    const auto tool = part.Features().FindByBody(b->Guid)->Id();
    Body* fused =
        part.AddBoolean(boolean::BooleanOp::Union, target, tool, "Fuse");
    ASSERT_NE(fused, nullptr);

    RigidTransform t{.Translation = Point3d{1, 0, 0}};
    Body* copy = part.DuplicateBody(fused->Guid, t, "FuseCopy");
    ASSERT_NE(copy, nullptr);
    const Guid copyGuid = copy->Guid;
    const Point3d copyPt = FirstVertex(*copy);

    const fs::path path =
        fs::temp_directory_path() / "brep_test_copied_body_roundtrip.xl";
    auto saved = io::SaveXl(*doc, path);
    ASSERT_TRUE(saved.Ok) << saved.Error;

    auto loaded = io::LoadXl(path);
    ASSERT_TRUE(loaded.Ok()) << loaded.Error;
    Part* p2 = loaded.document->MainPart();
    ASSERT_NE(p2, nullptr);
    Body* copy2 = p2->FindBody(copyGuid);
    ASSERT_NE(copy2, nullptr);
    auto* feat = p2->Features().FindByBody(copyGuid);
    ASSERT_NE(feat, nullptr);
    EXPECT_EQ(feat->TypeName(), "CopiedBody");
    EXPECT_NEAR(FirstVertex(*copy2).x(), copyPt.x(), 1e-6);

    std::error_code ec;
    fs::remove(path, ec);
    fs::remove(io::BksCachePathFor(path), ec);
}

TEST(CopiedBody, RejectsRotation)
{
    auto doc = Document::Create("copy_rot",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");
    Body* box = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
    ASSERT_NE(box, nullptr);
    RigidTransform t;
    t.XAxis = Vector3d{0, 1, 0};
    t.YAxis = Vector3d{-1, 0, 0};
    EXPECT_EQ(part.DuplicateBody(box->Guid, t), nullptr);
}

TEST(CopiedBody, TransformBodyMovesBoxAndUndo)
{
    auto doc = Document::Create("xf_box",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");
    Body* box = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
    ASSERT_NE(box, nullptr);
    const Guid guid = box->Guid;
    RigidTransform t{.Translation = Point3d{2, 0, 0}};
    ASSERT_TRUE(part.TransformBody(guid, t));
    auto* feat = part.Features().FindByBody(guid);
    ASSERT_NE(feat, nullptr);
    auto spec = feat->ToPrimitiveSpec(part.Parameters());
    ASSERT_TRUE(spec.has_value());
    const auto* boxSpec = std::get_if<BoxSpec>(&*spec);
    ASSERT_NE(boxSpec, nullptr);
    EXPECT_NEAR(boxSpec->Min.x(), 2.0, 1e-9);

    ASSERT_TRUE(part.FeatureHistory().Undo(part));
    feat = part.Features().FindByBody(guid);
    ASSERT_NE(feat, nullptr);
    spec = feat->ToPrimitiveSpec(part.Parameters());
    ASSERT_TRUE(spec.has_value());
    boxSpec = std::get_if<BoxSpec>(&*spec);
    ASSERT_NE(boxSpec, nullptr);
    EXPECT_NEAR(boxSpec->Min.x(), 0.0, 1e-9);
    EXPECT_NE(part.FindBody(guid), nullptr);
}

TEST(CopiedBody, TransformBodyRejectsBoolean)
{
    auto doc = Document::Create("xf_bool",
                                boolean::MakeDefaultBooleanEvaluator());
    Part& part = doc->AddPart("Main");
    Body* a = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
    Body* b = part.AddBox(BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}});
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const auto target = part.Features().FindByBody(a->Guid)->Id();
    const auto tool = part.Features().FindByBody(b->Guid)->Id();
    Body* fused =
        part.AddBoolean(boolean::BooleanOp::Union, target, tool, "Fuse");
    ASSERT_NE(fused, nullptr);
    const Guid fusedGuid = fused->Guid;
    RigidTransform t{.Translation = Point3d{1, 0, 0}};
    EXPECT_FALSE(part.TransformBody(fusedGuid, t));
    EXPECT_NE(part.FindBody(fusedGuid), nullptr);
}

}  // namespace
}  // namespace brep
