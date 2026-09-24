#include "adapter/SceneAdapter.h"

#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/Aspect.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace brep::viewer::adapter
{
namespace
{

TEST(SceneAdapter, AddPrimitiveEmitsOneSceneEvent)
{
    std::vector<std::string> sites;
    class SiteAspect final : public brep::IAspect
    {
     public:
        explicit SiteAspect(std::vector<std::string>* log)
            : m_log(log)
        {
        }
        std::string_view Name() const noexcept override
        {
            return "sites";
        }
        void Before(const brep::AspectEvent&) override {}
        void After(const brep::AspectEvent& event) override
        {
            if (event.Site == "scene.addPrimitive")
            {
                m_log->emplace_back(event.Subject);
            }
        }
        void OnError(const brep::AspectEvent&, const std::exception&) override {}

     private:
        std::vector<std::string>* m_log;
    };

    brep::AspectChain chain;
    chain.Add(std::make_unique<SiteAspect>(&sites));
    brep::ScopedProcessAspectChain guard(std::move(chain));

    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* body = scene.AddPrimitive(BoxSpec{
        .Min = Point3d{0.0, 0.0, 0.0},
        .Max = Point3d{1.0, 1.0, 1.0},
        .Name = "box",
    });
    ASSERT_NE(body, nullptr);
    ASSERT_EQ(sites.size(), 1u);
    EXPECT_EQ(sites[0], "box");
}

TEST(SceneAdapter, CreateBlankHasMainPart)
{
    auto doc = SceneAdapter::CreateBlank("UnitTest");
    ASSERT_NE(doc, nullptr);
    SceneAdapter scene(doc.get());
    ASSERT_NE(scene.MainPart(), nullptr);
    EXPECT_EQ(doc->Name, "UnitTest");
}

TEST(SceneAdapter, AddBoxMeshAndObject)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    BoxSpec spec{
        .Min = Point3d{0.0, 0.0, 0.0},
        .Max = Point3d{2.0, 3.0, 4.0},
        .Name = "box",
    };
    Body* body = scene.AddPrimitive(spec);
    ASSERT_NE(body, nullptr);

    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->TypeName, "Box");
    EXPECT_EQ(obj->BodyGuid, body->Guid);
    auto loaded = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(loaded.has_value());
    const auto* box = std::get_if<BoxSpec>(&*loaded);
    ASSERT_NE(box, nullptr);
    EXPECT_DOUBLE_EQ(box->Max.x() - box->Min.x(), 2.0);
    EXPECT_DOUBLE_EQ(box->Max.y() - box->Min.y(), 3.0);
    EXPECT_DOUBLE_EQ(box->Max.z() - box->Min.z(), 4.0);

    auto mesh = scene.MeshForBody(body->Guid);
    EXPECT_FALSE(mesh.Faces.Vertices.empty());
    EXPECT_FALSE(mesh.Edges.Positions.empty());
}

TEST(SceneAdapter, SetPrimitiveBoxAndUndo)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    Body* body = scene.AddPrimitive(BoxSpec{
        .Min = Point3d{0.0, 0.0, 0.0},
        .Max = Point3d{1.0, 1.0, 1.0},
        .Name = "box",
    });
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    scene.RecordAppendPrimitive(feat::FeatureId{obj->FeatureGuid},
                                BoxSpec{.Min = Point3d{0, 0, 0},
                                        .Max = Point3d{1, 1, 1},
                                        .Name = "box"});

    ASSERT_TRUE(scene.SetPrimitive(
        feat::FeatureId{obj->FeatureGuid},
        BoxSpec{.Min = {0, 0, 0}, .Max = {5.0, 7.0, 6.0}, .Name = "box"}));
    auto spec = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(spec.has_value());
    const auto* box = std::get_if<BoxSpec>(&*spec);
    ASSERT_NE(box, nullptr);
    EXPECT_DOUBLE_EQ(box->Max.x() - box->Min.x(), 5.0);
    EXPECT_DOUBLE_EQ(box->Max.z() - box->Min.z(), 6.0);
    EXPECT_DOUBLE_EQ(box->Max.y() - box->Min.y(), 7.0);

    // EditParameters is top of stack; undo restores original L/W/H.
    scene.UndoFeature();
    spec = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(spec.has_value());
    box = std::get_if<BoxSpec>(&*spec);
    ASSERT_NE(box, nullptr);
    EXPECT_DOUBLE_EQ(box->Max.x() - box->Min.x(), 1.0);
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 1u);

    // Second undo removes the AppendFeature box.
    scene.UndoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 0u);
    scene.RedoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 1u);
}

TEST(SceneAdapter, SetPrimitiveRejectsMismatchedSpec)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* body = scene.AddPrimitive(BoxSpec{
        .Min = {0, 0, 0},
        .Max = {1, 1, 1},
        .Name = "box",
    });
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    EXPECT_FALSE(scene.SetPrimitive(
        feat::FeatureId{obj->FeatureGuid},
        SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "nope"}));
}

TEST(SceneAdapter, RemoveFeatureAndUndo)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* body = scene.AddPrimitive(BoxSpec{
        .Min = Point3d{0.0, 0.0, 0.0},
        .Max = Point3d{1.0, 1.0, 1.0},
        .Name = "box",
    });
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());

    ASSERT_TRUE(scene.RemoveFeature(feat::FeatureId{obj->FeatureGuid}));
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 0u);

    scene.UndoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 1u);
    scene.RedoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 0u);
}

TEST(SceneAdapter, SpecForBox)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* body = scene.AddPrimitive(BoxSpec{
        .Min = Point3d{1.0, 2.0, 3.0},
        .Max = Point3d{2.0, 4.0, 5.0},
        .Name = "src",
    });
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());

    auto spec = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(spec.has_value());
    const auto* box = std::get_if<BoxSpec>(&*spec);
    ASSERT_NE(box, nullptr);
    EXPECT_DOUBLE_EQ(box->Min.x(), 1.0);
    EXPECT_DOUBLE_EQ(box->Max.y(), 4.0);
}

TEST(SceneAdapter, AddSphereMeshAndObject)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    Body* body = scene.AddPrimitive(SphereSpec{
        .Center = Point3d{1.0, 2.0, 3.0},
        .Radius = 2.5,
        .Name = "sphere",
    });
    ASSERT_NE(body, nullptr);

    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->TypeName, "Sphere");
    EXPECT_EQ(obj->BodyGuid, body->Guid);
    auto loaded = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(loaded.has_value());
    const auto* sphere = std::get_if<SphereSpec>(&*loaded);
    ASSERT_NE(sphere, nullptr);
    EXPECT_DOUBLE_EQ(sphere->Radius, 2.5);

    auto mesh = scene.MeshForBody(body->Guid);
    EXPECT_FALSE(mesh.Faces.Vertices.empty());
    EXPECT_FALSE(mesh.Faces.Indices.empty());
    // Analytic sphere seam is hidden by default (T1.4).
    EXPECT_TRUE(mesh.Edges.Positions.empty());

    // Sample a mesh normal: should be outward from center.
    const MeshVertex& mv = mesh.Faces.Vertices.front();
    EXPECT_NEAR((mv.Position - Point3d{1.0, 2.0, 3.0}).norm(), 2.5, 1e-5);
    EXPECT_GT(mv.Normal.dot(mv.Position - Point3d{1.0, 2.0, 3.0}), 0.0);
}

TEST(SceneAdapter, SetPrimitiveSphereAndUndo)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    Body* body = scene.AddPrimitive(SphereSpec{
        .Center = Point3d{0.0, 0.0, 0.0},
        .Radius = 1.0,
        .Name = "sphere",
    });
    ASSERT_NE(body, nullptr);
    const Guid bodyGuid = body->Guid;
    auto obj = scene.ObjectForBody(bodyGuid);
    ASSERT_TRUE(obj.has_value());
    scene.RecordAppendPrimitive(feat::FeatureId{obj->FeatureGuid},
                               SphereSpec{.Center = {0, 0, 0},
                                          .Radius = 1.0,
                                          .Name = "sphere"});

    ASSERT_TRUE(scene.SetPrimitive(
        feat::FeatureId{obj->FeatureGuid},
        SphereSpec{.Center = {0, 0, 0}, .Radius = 3.5, .Name = "sphere"}));
    auto spec = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(spec.has_value());
    const auto* sphere = std::get_if<SphereSpec>(&*spec);
    ASSERT_NE(sphere, nullptr);
    EXPECT_DOUBLE_EQ(sphere->Radius, 3.5);

    auto mesh = scene.MeshForBody(bodyGuid);
    ASSERT_FALSE(mesh.Faces.Vertices.empty());
    EXPECT_NEAR(
        (mesh.Faces.Vertices.front().Position - Point3d{0, 0, 0}).norm(),
        3.5, 1e-4);

    scene.UndoFeature();
    spec = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(spec.has_value());
    sphere = std::get_if<SphereSpec>(&*spec);
    ASSERT_NE(sphere, nullptr);
    EXPECT_DOUBLE_EQ(sphere->Radius, 1.0);
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 1u);

    scene.UndoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 0u);
    scene.RedoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 1u);
}

TEST(SceneAdapter, SpecForSphere)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* body = scene.AddPrimitive(SphereSpec{
        .Center = Point3d{1.0, 2.0, 3.0},
        .Radius = 2.5,
        .Name = "ball",
    });
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());

    auto spec = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(spec.has_value());
    const auto* sphere = std::get_if<SphereSpec>(&*spec);
    ASSERT_NE(sphere, nullptr);
    EXPECT_DOUBLE_EQ(sphere->Center.x(), 1.0);
    EXPECT_DOUBLE_EQ(sphere->Center.y(), 2.0);
    EXPECT_DOUBLE_EQ(sphere->Center.z(), 3.0);
    EXPECT_DOUBLE_EQ(sphere->Radius, 2.5);

    auto byBody = scene.SpecFor(Guid{}, body->Guid);
    ASSERT_TRUE(byBody.has_value());
    const auto* byBodySphere = std::get_if<SphereSpec>(&*byBody);
    ASSERT_NE(byBodySphere, nullptr);
    EXPECT_DOUBLE_EQ(byBodySphere->Radius, 2.5);
    EXPECT_EQ(std::get_if<BoxSpec>(&*byBody), nullptr);
}

TEST(SceneAdapter, AddBooleanUnionSuppressesOperands)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    Body* a = scene.AddPrimitive(
        BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
    Body* b = scene.AddPrimitive(
        BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const Guid aGuid = a->Guid;
    const Guid bGuid = b->Guid;
    const auto target = scene.ObjectForBody(aGuid)->FeatureGuid;
    const auto tool = scene.ObjectForBody(bGuid)->FeatureGuid;

    Body* result = scene.AddBoolean(boolean::BooleanOp::Union,
                                     feat::FeatureId{target},
                                     feat::FeatureId{tool}, "Fuse");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 1u);
    EXPECT_EQ(scene.MainPart()->FindBody(aGuid), nullptr);
    EXPECT_EQ(scene.MainPart()->FindBody(bGuid), nullptr);

    auto obj = scene.ObjectForBody(result->Guid);
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->TypeName, "Boolean");
    EXPECT_FALSE(scene.SpecFor(obj->FeatureGuid, result->Guid).has_value());

    auto params = scene.BooleanParamsFor(feat::FeatureId{obj->FeatureGuid});
    ASSERT_TRUE(params.has_value());
    EXPECT_EQ(params->Op, boolean::BooleanOp::Union);

    scene.UndoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 2u);
    EXPECT_NE(scene.MainPart()->FindBody(aGuid), nullptr);
    EXPECT_NE(scene.MainPart()->FindBody(bGuid), nullptr);

    scene.RedoFeature();
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 1u);
}

TEST(SceneAdapter, DuplicateBodyCopiesBoolean)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    Body* a = scene.AddPrimitive(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
    Body* b = scene.AddPrimitive(
        BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const auto target = scene.ObjectForBody(a->Guid)->FeatureGuid;
    const auto tool = scene.ObjectForBody(b->Guid)->FeatureGuid;
    Body* fused = scene.AddBoolean(boolean::BooleanOp::Union,
                                    feat::FeatureId{target},
                                    feat::FeatureId{tool}, "Fuse");
    ASSERT_NE(fused, nullptr);
    const Guid srcGuid = fused->Guid;

    RigidTransform t{.Translation = Point3d{0, 5, 0}};
    Body* copy = scene.DuplicateBody(srcGuid, t);
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy->Guid, srcGuid);
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 2u);
    EXPECT_FALSE(scene.SpecFor(Guid{}, copy->Guid).has_value());

    auto obj = scene.ObjectForBody(copy->Guid);
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->TypeName, "CopiedBody");

    scene.UndoFeature();
    EXPECT_EQ(scene.MainPart()->FindBody(copy->Guid), nullptr);
    EXPECT_NE(scene.MainPart()->FindBody(srcGuid), nullptr);
}

TEST(SceneAdapter, TransformBodyMovesBox)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* box = scene.AddPrimitive(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
    ASSERT_NE(box, nullptr);
    const Guid guid = box->Guid;
    RigidTransform t{.Translation = Point3d{2, 0, 0}};
    ASSERT_TRUE(scene.TransformBody(guid, t));
    auto spec = scene.SpecFor(Guid{}, guid);
    ASSERT_TRUE(spec.has_value());
    const auto* boxSpec = std::get_if<BoxSpec>(&*spec);
    ASSERT_NE(boxSpec, nullptr);
    EXPECT_NEAR(boxSpec->Min.x(), 2.0, 1e-9);
}

TEST(SceneAdapter, TransformBodyMovesSphere)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* sphere = scene.AddPrimitive(
        SphereSpec{.Center = {0, 0, 0}, .Radius = 1.5, .Name = "S"});
    ASSERT_NE(sphere, nullptr);
    const Guid guid = sphere->Guid;
    RigidTransform t{.Translation = Point3d{0, 3, 0}};
    ASSERT_TRUE(scene.TransformBody(guid, t));
    auto spec = scene.SpecFor(Guid{}, guid);
    ASSERT_TRUE(spec.has_value());
    const auto* sphereSpec = std::get_if<SphereSpec>(&*spec);
    ASSERT_NE(sphereSpec, nullptr);
    EXPECT_NEAR(sphereSpec->Center.y(), 3.0, 1e-9);
    EXPECT_NEAR(sphereSpec->Radius, 1.5, 1e-9);
}

TEST(SceneAdapter, TransformBodyUndoRestoresBox)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* box = scene.AddPrimitive(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
    ASSERT_NE(box, nullptr);
    const Guid guid = box->Guid;
    RigidTransform t{.Translation = Point3d{2, 0, 0}};
    ASSERT_TRUE(scene.TransformBody(guid, t));
    scene.UndoFeature();
    auto spec = scene.SpecFor(Guid{}, guid);
    ASSERT_TRUE(spec.has_value());
    const auto* boxSpec = std::get_if<BoxSpec>(&*spec);
    ASSERT_NE(boxSpec, nullptr);
    EXPECT_NEAR(boxSpec->Min.x(), 0.0, 1e-9);
    EXPECT_NE(scene.MainPart()->FindBody(guid), nullptr);
}

TEST(SceneAdapter, TransformBodyRejectsBoolean)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* a = scene.AddPrimitive(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
    Body* b = scene.AddPrimitive(
        BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const auto target = scene.ObjectForBody(a->Guid)->FeatureGuid;
    const auto tool = scene.ObjectForBody(b->Guid)->FeatureGuid;
    Body* fused = scene.AddBoolean(boolean::BooleanOp::Union,
                                    feat::FeatureId{target},
                                    feat::FeatureId{tool}, "Fuse");
    ASSERT_NE(fused, nullptr);
    const Guid fusedGuid = fused->Guid;
    RigidTransform t{.Translation = Point3d{1, 0, 0}};
    EXPECT_FALSE(scene.TransformBody(fusedGuid, t));
    EXPECT_NE(scene.MainPart()->FindBody(fusedGuid), nullptr);
}

TEST(SceneAdapter, CopyBoxAndSphereViaSpec)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    Body* box = scene.AddPrimitive(
        BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
    Body* sphere = scene.AddPrimitive(
        SphereSpec{.Center = {3, 0, 0}, .Radius = 0.75, .Name = "sphere"});
    ASSERT_NE(box, nullptr);
    ASSERT_NE(sphere, nullptr);
    const Guid boxGuid = box->Guid;
    const Guid sphereGuid = sphere->Guid;

    EXPECT_TRUE(scene.MeshForBody(sphereGuid).Edges.Positions.empty());

    RigidTransform t{.Translation = Point3d{0, 2, 0}};
    auto boxSpec = scene.SpecFor(Guid{}, boxGuid);
    auto sphereSpec = scene.SpecFor(Guid{}, sphereGuid);
    ASSERT_TRUE(boxSpec.has_value());
    ASSERT_TRUE(sphereSpec.has_value());

    auto movedBox = ApplyTransform(*boxSpec, t);
    auto movedSphere = ApplyTransform(*sphereSpec, t);
    ASSERT_TRUE(movedBox.has_value());
    ASSERT_TRUE(movedSphere.has_value());

    Body* boxCopy = scene.AddPrimitive(*movedBox);
    Body* sphereCopy = scene.AddPrimitive(*movedSphere);
    ASSERT_NE(boxCopy, nullptr);
    ASSERT_NE(sphereCopy, nullptr);
    auto boxObj = scene.ObjectForBody(boxCopy->Guid);
    auto sphereObj = scene.ObjectForBody(sphereCopy->Guid);
    ASSERT_TRUE(boxObj.has_value());
    ASSERT_TRUE(sphereObj.has_value());
    scene.RecordAppendPrimitive(feat::FeatureId{boxObj->FeatureGuid}, *movedBox);
    scene.RecordAppendPrimitive(feat::FeatureId{sphereObj->FeatureGuid},
                                *movedSphere);

    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 4u);

    auto copiedSphereSpec = scene.SpecFor(Guid{}, sphereCopy->Guid);
    ASSERT_TRUE(copiedSphereSpec.has_value());
    const auto* s = std::get_if<SphereSpec>(&*copiedSphereSpec);
    ASSERT_NE(s, nullptr);
    EXPECT_NEAR(s->Center.y(), 2.0, 1e-9);

    scene.UndoFeature(2);
    EXPECT_EQ(scene.MainPart()->Model().Bodies().size(), 2u);
    EXPECT_NE(scene.MainPart()->FindBody(boxGuid), nullptr);
    EXPECT_NE(scene.MainPart()->FindBody(sphereGuid), nullptr);
}

TEST(SceneAdapter, SpecForBezier)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* body = scene.AddPrimitive(BezierSpec{
        .Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
        .Name = "bezier",
    });
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->Type, BodyType::Wire);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->TypeName, "Bezier");
    auto spec = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(spec.has_value());
    const auto* bezier = std::get_if<BezierSpec>(&*spec);
    ASSERT_NE(bezier, nullptr);
    EXPECT_NEAR(bezier->P1().y(), 1.0, 1e-9);
    auto mesh = scene.MeshForBody(body->Guid);
    EXPECT_TRUE(mesh.Faces.Vertices.empty());
    EXPECT_GE(mesh.Edges.Positions.size(), 64u);
}

TEST(SceneAdapter, SetPrimitiveBezierUndoRedo)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    Body* body = scene.AddPrimitive(BezierSpec{
        .Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
        .Name = "bezier",
    });
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    const feat::FeatureId fid{obj->FeatureGuid};
    scene.RecordAppendPrimitive(fid, BezierSpec{
                                         .Cvs = {{0, 0, 0},
                                                 {0, 1, 0},
                                                 {1, 1, 0},
                                                 {1, 0, 0}},
                                         .Name = "bezier",
                                     });

    BezierSpec edited{
        .Cvs = {{0, 0, 0}, {0, 2, 0}, {1, 1, 0}, {1, 0, 0}},
        .Name = "bezier",
    };
    ASSERT_TRUE(scene.SetPrimitive(fid, edited));
    auto after = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(after.has_value());
    EXPECT_NEAR(std::get<BezierSpec>(*after).P1().y(), 2.0, 1e-9);

    scene.UndoFeature();
    auto undone = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(undone.has_value());
    EXPECT_NEAR(std::get<BezierSpec>(*undone).P1().y(), 1.0, 1e-9);

    scene.RedoFeature();
    auto redone = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(redone.has_value());
    EXPECT_NEAR(std::get<BezierSpec>(*redone).P1().y(), 2.0, 1e-9);
}

TEST(SceneAdapter, SetPrimitiveBezierWeightsUndoRedo)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    BezierSpec spec{
        .Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
        .Weights = {1.0, 1.0, 1.0, 1.0},
        .Name = "bezier",
    };
    Body* body = scene.AddPrimitive(spec);
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    const feat::FeatureId fid{obj->FeatureGuid};
    scene.RecordAppendPrimitive(fid, spec);

    BezierSpec edited = spec;
    edited.Weights = {1.0, 2.0, 1.0, 1.0};
    ASSERT_TRUE(scene.SetPrimitive(fid, edited));
    auto after = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(after.has_value());
    EXPECT_NEAR(std::get<BezierSpec>(*after).Weights[1], 2.0, 1e-9);

    scene.UndoFeature();
    auto undone = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(undone.has_value());
    EXPECT_NEAR(std::get<BezierSpec>(*undone).Weights[1], 1.0, 1e-9);
}

TEST(SceneAdapter, SetPrimitiveNurbsWeightsUndoRedo)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());
    NurbsCurveSpec spec{
        .Cvs = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}},
        .Weights = {1.0, 1.0, 1.0, 1.0},
        .Name = "nurbs",
    };
    Body* body = scene.AddPrimitive(spec);
    ASSERT_NE(body, nullptr);
    auto obj = scene.ObjectForBody(body->Guid);
    ASSERT_TRUE(obj.has_value());
    const feat::FeatureId fid{obj->FeatureGuid};
    scene.RecordAppendPrimitive(fid, spec);

    auto initial = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(initial.has_value());
    ASSERT_EQ(std::get<NurbsCurveSpec>(*initial).Cvs.size(), 4u);

    NurbsCurveSpec edited = std::get<NurbsCurveSpec>(*initial);
    edited.Weights[1] = 2.0;
    ASSERT_TRUE(scene.SetPrimitive(fid, edited));
    auto after = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(after.has_value());
    EXPECT_NEAR(std::get<NurbsCurveSpec>(*after).Weights[1], 2.0, 1e-9);

    scene.UndoFeature();
    auto undone = scene.SpecFor(obj->FeatureGuid, Guid{});
    ASSERT_TRUE(undone.has_value());
    EXPECT_NEAR(std::get<NurbsCurveSpec>(*undone).Weights[1], 1.0, 1e-9);
}

TEST(SceneAdapter, AddBooleanSubtractPrimaryIsTarget)
{
    auto doc = SceneAdapter::CreateBlank();
    SceneAdapter scene(doc.get());

    Body* a = scene.AddPrimitive(
        BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 1}, .Name = "A"});
    Body* b = scene.AddPrimitive(
        BoxSpec{.Min = {1, 1, 0}, .Max = {2, 2, 1}, .Name = "B"});
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const auto target = scene.ObjectForBody(a->Guid)->FeatureGuid;
    const auto tool = scene.ObjectForBody(b->Guid)->FeatureGuid;

    Body* result = scene.AddBoolean(boolean::BooleanOp::Subtract,
                                     feat::FeatureId{target},
                                     feat::FeatureId{tool}, "Cut");
    ASSERT_NE(result, nullptr);
    auto obj = scene.ObjectForBody(result->Guid);
    ASSERT_TRUE(obj.has_value());
    EXPECT_EQ(obj->TypeName, "Boolean");
    auto params = scene.BooleanParamsFor(feat::FeatureId{obj->FeatureGuid});
    ASSERT_TRUE(params.has_value());
    EXPECT_EQ(params->Op, boolean::BooleanOp::Subtract);
    ASSERT_FALSE(result->Shells.empty());
    EXPECT_GT(result->Shells[0]->Faces.size(), 6u);
}

}  // namespace
}  // namespace brep::viewer::adapter
