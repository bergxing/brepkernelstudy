#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/BooleanBuilder.h"
#include "brep/bool/FaceSelector.h"
#include "brep/bool/TopologyCopy.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(TopologyCopy, CopyBoxFaceBuildsValidShell)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
  ASSERT_NE(box, nullptr);
  ASSERT_FALSE(box->Shells.empty());
  ASSERT_FALSE(box->Shells[0]->Faces.empty());

  boolean::TopologyCopyContext ctx{model};
  Face* copied =
      boolean::CopyFaceSubgraph(ctx, *box->Shells[0]->Faces.front(), false);
  ASSERT_NE(copied, nullptr);

  Body* body = model.MakeBody(BodyType::Solid, "single_face");
  Shell* shell = model.MakeShell(false, "single_face_shell");
  body->Shells.push_back(shell);
  shell->Faces.push_back(copied);
  EXPECT_TRUE(ValidateBody(*body).Ok());
}

TEST(BooleanBuilder, DisjointUnionBuildsValidBody)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  std::vector<boolean::FaceClassification> aVsB;
  std::vector<boolean::FaceClassification> bVsA;
  for (Shell* shell : a->Shells)
  {
    for (Face* face : shell->Faces)
    {
      aVsB.push_back({face, boolean::FaceRegion::Out});
    }
  }
  for (Shell* shell : b->Shells)
  {
    for (Face* face : shell->Faces)
    {
      bVsA.push_back({face, boolean::FaceRegion::Out});
    }
  }

  const boolean::FaceSelection sel =
      boolean::SelectCsgFaces(boolean::BooleanOp::Union, aVsB, bVsA);
  const boolean::BooleanBuildResult built =
      boolean::BuildBooleanBody(model, boolean::BooleanOp::Union, sel, "union_copy");
  EXPECT_TRUE(built.Diagnostics.empty()) << built.Diagnostics;
  ASSERT_NE(built.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*built.OutputBody).Ok());
}

TEST(TopologyCopy, CopyBoxThenTranslateKeepsSource)
{
    Model model;
    Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
    ASSERT_NE(box, nullptr);

    boolean::TopologyCopyContext ctx{model};
    Body* copied = boolean::CopyBodySubgraph(ctx, *box, "_copy");
    ASSERT_NE(copied, nullptr);
    ASSERT_FALSE(ctx.Points.empty());

    const Point* srcPt = ctx.Points.begin()->first;
    Point* dstPt = ctx.Points.begin()->second;
    ASSERT_NE(srcPt, nullptr);
    ASSERT_NE(dstPt, nullptr);
    const Point3d srcBefore = srcPt->Xyz();
    const Point3d dstBefore = dstPt->Xyz();
    EXPECT_NEAR(srcBefore.x(), dstBefore.x(), 1e-12);

    RigidTransform t{.Translation = Point3d{5, 0, 0}};
    ASSERT_TRUE(boolean::ApplyTransformToCopied(ctx, t));
    EXPECT_NEAR(srcPt->Xyz().x(), srcBefore.x(), 1e-12);
    EXPECT_NEAR(dstPt->Xyz().x(), dstBefore.x() + 5.0, 1e-12);
    EXPECT_TRUE(ValidateBody(*copied).Ok());
    EXPECT_TRUE(ValidateBody(*box).Ok());
}

}  // namespace
}  // namespace brep
