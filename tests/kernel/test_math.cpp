#include "brep/math.hpp"

#include <gtest/gtest.h>

namespace brep {
namespace {

TEST(MathSmoke, Point3dBasics) {
  Point3d p{1.0, 2.0, 3.0};
  EXPECT_DOUBLE_EQ(p.x(), 1.0);
  EXPECT_DOUBLE_EQ(p.y(), 2.0);
  EXPECT_DOUBLE_EQ(p.z(), 3.0);
}

TEST(MathSmoke, Vector3dNorm) {
  Vector3d v{3.0, 4.0, 0.0};
  EXPECT_DOUBLE_EQ(v.norm(), 5.0);
}

}  // namespace
}  // namespace brep
