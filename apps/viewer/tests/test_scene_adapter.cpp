#include "adapter/scene_adapter.hpp"

#include "api/core.hpp"
#include "api/modeling.hpp"

#include <gtest/gtest.h>

namespace brep::viewer::adapter {
namespace {

TEST(SceneAdapter, CreateBlankHasMainPart) {
  auto doc = SceneAdapter::create_blank("UnitTest");
  ASSERT_NE(doc, nullptr);
  SceneAdapter scene(doc.get());
  ASSERT_NE(scene.main_part(), nullptr);
  EXPECT_EQ(doc->name, "UnitTest");
}

TEST(SceneAdapter, AddBoxMeshAndObject) {
  auto doc = SceneAdapter::create_blank();
  SceneAdapter scene(doc.get());

  BoxSpec spec{
      .min = Point3d{0.0, 0.0, 0.0},
      .max = Point3d{2.0, 3.0, 4.0},
      .name = "box",
  };
  Body* body = scene.add_box(spec);
  ASSERT_NE(body, nullptr);

  auto obj = scene.object_for_body(body->guid);
  ASSERT_TRUE(obj.has_value());
  EXPECT_EQ(obj->type_name, "Box");
  EXPECT_EQ(obj->body_guid, body->guid);
  ASSERT_TRUE(obj->box.has_value());
  EXPECT_DOUBLE_EQ(obj->box->length, 2.0);
  EXPECT_DOUBLE_EQ(obj->box->height, 3.0);
  EXPECT_DOUBLE_EQ(obj->box->width, 4.0);

  auto mesh = scene.mesh_for_body(body->guid);
  EXPECT_FALSE(mesh.faces.vertices.empty());
  EXPECT_FALSE(mesh.edges.positions.empty());
}

TEST(SceneAdapter, SetBoxParamsAndUndo) {
  auto doc = SceneAdapter::create_blank();
  SceneAdapter scene(doc.get());

  Body* body = scene.add_box(BoxSpec{
      .min = Point3d{0.0, 0.0, 0.0},
      .max = Point3d{1.0, 1.0, 1.0},
      .name = "box",
  });
  ASSERT_NE(body, nullptr);
  auto obj = scene.object_for_body(body->guid);
  ASSERT_TRUE(obj.has_value());
  scene.record_append_feature(feat::FeatureId{obj->feature_guid},
                              BoxSpec{.min = Point3d{0, 0, 0},
                                      .max = Point3d{1, 1, 1},
                                      .name = "box"});

  ASSERT_TRUE(scene.set_box_params(feat::FeatureId{obj->feature_guid},
                                   BoxParams{5.0, 6.0, 7.0}));
  auto params = scene.box_params(feat::FeatureId{obj->feature_guid});
  ASSERT_TRUE(params.has_value());
  EXPECT_DOUBLE_EQ(params->length, 5.0);
  EXPECT_DOUBLE_EQ(params->width, 6.0);
  EXPECT_DOUBLE_EQ(params->height, 7.0);

  // EditParameters is top of stack; undo restores original L/W/H.
  scene.undo_feature();
  params = scene.box_params(feat::FeatureId{obj->feature_guid});
  ASSERT_TRUE(params.has_value());
  EXPECT_DOUBLE_EQ(params->length, 1.0);
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 1u);

  // Second undo removes the AppendFeature box.
  scene.undo_feature();
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 0u);
  scene.redo_feature();
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 1u);
}

TEST(SceneAdapter, RemoveFeatureAndUndo) {
  auto doc = SceneAdapter::create_blank();
  SceneAdapter scene(doc.get());
  Body* body = scene.add_box(BoxSpec{
      .min = Point3d{0.0, 0.0, 0.0},
      .max = Point3d{1.0, 1.0, 1.0},
      .name = "box",
  });
  ASSERT_NE(body, nullptr);
  auto obj = scene.object_for_body(body->guid);
  ASSERT_TRUE(obj.has_value());

  ASSERT_TRUE(scene.remove_feature(feat::FeatureId{obj->feature_guid}));
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 0u);

  scene.undo_feature();
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 1u);
  scene.redo_feature();
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 0u);
}

TEST(SceneAdapter, BoxSpecForFeature) {
  auto doc = SceneAdapter::create_blank();
  SceneAdapter scene(doc.get());
  Body* body = scene.add_box(BoxSpec{
      .min = Point3d{1.0, 2.0, 3.0},
      .max = Point3d{2.0, 4.0, 5.0},
      .name = "src",
  });
  ASSERT_NE(body, nullptr);
  auto obj = scene.object_for_body(body->guid);
  ASSERT_TRUE(obj.has_value());

  auto spec = scene.box_spec_for(obj->feature_guid, Guid{});
  ASSERT_TRUE(spec.has_value());
  EXPECT_DOUBLE_EQ(spec->min.x(), 1.0);
  EXPECT_DOUBLE_EQ(spec->max.y(), 4.0);
}

TEST(SceneAdapter, AddSphereMeshAndObject) {
  auto doc = SceneAdapter::create_blank();
  SceneAdapter scene(doc.get());

  Body* body = scene.add_sphere(SphereSpec{
      .center = Point3d{1.0, 2.0, 3.0},
      .radius = 2.5,
      .name = "sphere",
  });
  ASSERT_NE(body, nullptr);

  auto obj = scene.object_for_body(body->guid);
  ASSERT_TRUE(obj.has_value());
  EXPECT_EQ(obj->type_name, "Sphere");
  EXPECT_EQ(obj->body_guid, body->guid);
  ASSERT_TRUE(obj->sphere.has_value());
  EXPECT_DOUBLE_EQ(obj->sphere->radius, 2.5);

  auto mesh = scene.mesh_for_body(body->guid);
  EXPECT_FALSE(mesh.faces.vertices.empty());
  EXPECT_FALSE(mesh.faces.indices.empty());
  // Analytic sphere seam is hidden by default (T1.4).
  EXPECT_TRUE(mesh.edges.positions.empty());

  // Sample a mesh normal: should be outward from center.
  const MeshVertex& mv = mesh.faces.vertices.front();
  EXPECT_NEAR((mv.position - Point3d{1.0, 2.0, 3.0}).norm(), 2.5, 1e-5);
  EXPECT_GT(mv.normal.dot(mv.position - Point3d{1.0, 2.0, 3.0}), 0.0);
}

TEST(SceneAdapter, SetSphereParamsAndUndo) {
  auto doc = SceneAdapter::create_blank();
  SceneAdapter scene(doc.get());

  Body* body = scene.add_sphere(SphereSpec{
      .center = Point3d{0.0, 0.0, 0.0},
      .radius = 1.0,
      .name = "sphere",
  });
  ASSERT_NE(body, nullptr);
  const Guid body_guid = body->guid;
  auto obj = scene.object_for_body(body_guid);
  ASSERT_TRUE(obj.has_value());
  scene.record_append_sphere(feat::FeatureId{obj->feature_guid},
                             SphereSpec{.center = {0, 0, 0},
                                        .radius = 1.0,
                                        .name = "sphere"});

  ASSERT_TRUE(scene.set_sphere_params(feat::FeatureId{obj->feature_guid},
                                      SphereParams{3.5}));
  auto params = scene.sphere_params(feat::FeatureId{obj->feature_guid});
  ASSERT_TRUE(params.has_value());
  EXPECT_DOUBLE_EQ(params->radius, 3.5);

  auto mesh = scene.mesh_for_body(body_guid);
  ASSERT_FALSE(mesh.faces.vertices.empty());
  EXPECT_NEAR((mesh.faces.vertices.front().position - Point3d{0, 0, 0}).norm(),
              3.5, 1e-4);

  scene.undo_feature();
  params = scene.sphere_params(feat::FeatureId{obj->feature_guid});
  ASSERT_TRUE(params.has_value());
  EXPECT_DOUBLE_EQ(params->radius, 1.0);
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 1u);

  scene.undo_feature();
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 0u);
  scene.redo_feature();
  EXPECT_EQ(scene.main_part()->model().bodies().size(), 1u);
}

}  // namespace
}  // namespace brep::viewer::adapter
