#include "api/Core.h"
#include "api/Modeling.h"
#include "api/Persistence.h"

#include "brep/bool/Boolean.h"
#include "brep/Mesh.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

namespace brep
{
namespace
{

[[nodiscard]] Part* LoadUntitledHexPadSphere(std::unique_ptr<Document>& document)
{
    const std::filesystem::path path{"C:/Users/xingbl/Desktop/untitled.xl"};
    if (!std::filesystem::exists(path))
    {
        return nullptr;
    }
    auto loaded = io::LoadXl(path);
    if (!loaded.Ok())
    {
        return nullptr;
    }
    document = std::move(loaded.document);
    return document != nullptr ? document->MainPart() : nullptr;
}

void ExpectUntitledHexPadSphere(Part& part, const feat::IFeature*& sphereFeat,
                                const feat::IFeature*& extrudeFeat)
{
    const Guid sphereGuid =
        Guid::FromString("0a680dea-ef99-4f03-944b-8252d7d16ee9");
    const Guid extrudeGuid =
        Guid::FromString("11de7787-cfcf-4e1e-98f2-5c5e866b68ee");
    ASSERT_NE(part.FindBody(sphereGuid), nullptr);
    ASSERT_NE(part.FindBody(extrudeGuid), nullptr);
    sphereFeat = part.Features().FindByBody(sphereGuid);
    extrudeFeat = part.Features().FindByBody(extrudeGuid);
    ASSERT_NE(sphereFeat, nullptr);
    ASSERT_NE(extrudeFeat, nullptr);
}

TEST(UntitledSphereExtrudeBoolean, SphereTargetUnionSucceeds)
{
    std::unique_ptr<Document> document;
    Part* part = LoadUntitledHexPadSphere(document);
    if (part == nullptr)
    {
        GTEST_SKIP() << "untitled.xl not found";
    }

    const feat::IFeature* sphereFeat = nullptr;
    const feat::IFeature* extrudeFeat = nullptr;
    ExpectUntitledHexPadSphere(*part, sphereFeat, extrudeFeat);

    Body* fused = part->AddBoolean(boolean::BooleanOp::Union, sphereFeat->Id(),
                                   extrudeFeat->Id(), "Fuse");
    ASSERT_NE(fused, nullptr) << part->LastRegenError();
    EXPECT_TRUE(ValidateBody(*fused).Ok());
}

TEST(UntitledSphereExtrudeBoolean, SphereTargetIntersectSucceeds)
{
    std::unique_ptr<Document> document;
    Part* part = LoadUntitledHexPadSphere(document);
    if (part == nullptr)
    {
        GTEST_SKIP() << "untitled.xl not found";
    }

    const feat::IFeature* sphereFeat = nullptr;
    const feat::IFeature* extrudeFeat = nullptr;
    ExpectUntitledHexPadSphere(*part, sphereFeat, extrudeFeat);

    Body* common = part->AddBoolean(boolean::BooleanOp::Intersect,
                                    sphereFeat->Id(), extrudeFeat->Id(),
                                    "Common");
    ASSERT_NE(common, nullptr) << part->LastRegenError();
    EXPECT_TRUE(ValidateBody(*common).Ok());
}

TEST(UntitledSphereExtrudeBoolean, PadMinusSphereSucceeds)
{
    std::unique_ptr<Document> document;
    Part* part = LoadUntitledHexPadSphere(document);
    if (part == nullptr)
    {
        GTEST_SKIP() << "untitled.xl not found";
    }

    const feat::IFeature* sphereFeat = nullptr;
    const feat::IFeature* extrudeFeat = nullptr;
    ExpectUntitledHexPadSphere(*part, sphereFeat, extrudeFeat);

    Body* cut = part->AddBoolean(boolean::BooleanOp::Subtract,
                                 extrudeFeat->Id(), sphereFeat->Id(), "Cut");
    ASSERT_NE(cut, nullptr) << part->LastRegenError();
    EXPECT_TRUE(ValidateBody(*cut).Ok());
    std::size_t sphereTriangles = 0;
    for (const Shell* shell : cut->Shells)
    {
        if (shell == nullptr)
        {
            continue;
        }
        for (const Face* face : shell->Faces)
        {
            if (face == nullptr || face->Surface == nullptr ||
                face->Surface->Kind() != SurfaceKind::Sphere)
            {
                continue;
            }
            TriangleMesh mesh;
            TessellateFace(*face, mesh);
            sphereTriangles += mesh.Indices.size() / 3U;
        }
    }
    EXPECT_GT(sphereTriangles, 0U);
}

TEST(UntitledSphereExtrudeBoolean, SphereMinusPadSucceeds)
{
    std::unique_ptr<Document> document;
    Part* part = LoadUntitledHexPadSphere(document);
    if (part == nullptr)
    {
        GTEST_SKIP() << "untitled.xl not found";
    }

    const feat::IFeature* sphereFeat = nullptr;
    const feat::IFeature* extrudeFeat = nullptr;
    ExpectUntitledHexPadSphere(*part, sphereFeat, extrudeFeat);

    Body* cut = part->AddBoolean(boolean::BooleanOp::Subtract, sphereFeat->Id(),
                                 extrudeFeat->Id(), "Cut");
    ASSERT_NE(cut, nullptr) << part->LastRegenError();
    EXPECT_TRUE(ValidateBody(*cut).Ok());
}

void ExpectUntitledTwoSpheres(Part& part, const feat::IFeature*& smallFeat,
                              const feat::IFeature*& largeFeat)
{
    const Guid smallGuid =
        Guid::FromString("223cdc9d-8455-46bf-a24f-49644c568c09");
    const Guid largeGuid =
        Guid::FromString("0a680dea-ef99-4f03-944b-8252d7d16ee9");
    ASSERT_NE(part.FindBody(smallGuid), nullptr);
    ASSERT_NE(part.FindBody(largeGuid), nullptr);
    smallFeat = part.Features().FindByBody(smallGuid);
    largeFeat = part.Features().FindByBody(largeGuid);
    ASSERT_NE(smallFeat, nullptr);
    ASSERT_NE(largeFeat, nullptr);
}

TEST(UntitledSphereSphereBoolean, UnionSucceeds)
{
    std::unique_ptr<Document> document;
    Part* part = LoadUntitledHexPadSphere(document);
    if (part == nullptr)
    {
        GTEST_SKIP() << "untitled.xl not found";
    }
    const feat::IFeature* smallFeat = nullptr;
    const feat::IFeature* largeFeat = nullptr;
    ExpectUntitledTwoSpheres(*part, smallFeat, largeFeat);
    Body* fused = part->AddBoolean(boolean::BooleanOp::Union, largeFeat->Id(),
                                   smallFeat->Id(), "Fuse");
    ASSERT_NE(fused, nullptr) << part->LastRegenError();
    EXPECT_TRUE(ValidateBody(*fused).Ok());
}

TEST(UntitledSphereSphereBoolean, IntersectSucceeds)
{
    std::unique_ptr<Document> document;
    Part* part = LoadUntitledHexPadSphere(document);
    if (part == nullptr)
    {
        GTEST_SKIP() << "untitled.xl not found";
    }
    const feat::IFeature* smallFeat = nullptr;
    const feat::IFeature* largeFeat = nullptr;
    ExpectUntitledTwoSpheres(*part, smallFeat, largeFeat);
    Body* common = part->AddBoolean(boolean::BooleanOp::Intersect,
                                    largeFeat->Id(), smallFeat->Id(),
                                    "Common");
    ASSERT_NE(common, nullptr) << part->LastRegenError();
    EXPECT_TRUE(ValidateBody(*common).Ok());
}

TEST(UntitledSphereSphereBoolean, LargeMinusSmallSucceeds)
{
    std::unique_ptr<Document> document;
    Part* part = LoadUntitledHexPadSphere(document);
    if (part == nullptr)
    {
        GTEST_SKIP() << "untitled.xl not found";
    }
    const feat::IFeature* smallFeat = nullptr;
    const feat::IFeature* largeFeat = nullptr;
    ExpectUntitledTwoSpheres(*part, smallFeat, largeFeat);
    Body* cut = part->AddBoolean(boolean::BooleanOp::Subtract, largeFeat->Id(),
                                 smallFeat->Id(), "Cut");
    ASSERT_NE(cut, nullptr) << part->LastRegenError();
    EXPECT_TRUE(ValidateBody(*cut).Ok());
}

}  // namespace
}  // namespace brep
