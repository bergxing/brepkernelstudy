#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/ops/Profile.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

[[nodiscard]] Face* FindFaceWithNormal(Body& body, const Vector3d& expected)
{
  if (body.Shells.empty() || !body.Shells[0])
  {
    return nullptr;
  }
  const Vector3d target = expected.normalized();
  for (Face* face : body.Shells[0]->Faces)
  {
    if (!face)
    {
      continue;
    }
    const Vector3d n = face->NormalAt(0.0, 0.0).normalized();
    if (n.dot(target) > 0.99)
    {
      return face;
    }
  }
  return nullptr;
}

TEST(ExtrudeWinding, ClockwiseTriangleHasOutwardNormals)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "pad_cw";
  spec.Distance = 1.0;
  spec.Plane = Plane::XzYUp();
  // Clockwise triangle (not an axis-aligned rectangle → prism path).
  spec.Profile.Outer = {
      Point2d{0, 0},
      Point2d{0, 2},
      Point2d{2, 0},
  };

  Body* body = ops::Extrude(model, spec);
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(ValidateBody(*body).Ok());

  EXPECT_NE(FindFaceWithNormal(*body, Vector3d{0, -1, 0}), nullptr);
  EXPECT_NE(FindFaceWithNormal(*body, Vector3d{0, 1, 0}), nullptr);
  EXPECT_NE(FindFaceWithNormal(*body, Vector3d{0, 0, -1}), nullptr);
  EXPECT_NE(FindFaceWithNormal(*body, Vector3d{-1, 0, 0}), nullptr);
  EXPECT_NE(FindFaceWithNormal(*body, Vector3d{1, 0, 1}.normalized()),
            nullptr);
}

}  // namespace
}  // namespace brep
