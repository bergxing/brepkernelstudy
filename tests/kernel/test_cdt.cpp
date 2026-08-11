#include "brep/mesh/cdt.hpp"

#include <gtest/gtest.h>

using brep::Point2d;
using brep::mesh::triangulate_constrained;

TEST(Cdt, UnconstrainedSquareTwoTriangles) {
  std::vector<Point2d> pts = {
      {0, 0}, {1, 0}, {1, 1}, {0, 1},
  };
  const auto r = triangulate_constrained(pts, {});
  ASSERT_TRUE(r.ok) << r.diagnostics;
  EXPECT_EQ(r.triangles.size(), 2u);
  // All vertices referenced in range
  for (const auto& t : r.triangles) {
    for (int k = 0; k < 3; ++k) {
      EXPECT_GE(t.v[k], 0);
      EXPECT_LT(t.v[k], static_cast<int>(r.vertices.size()));
    }
  }
}
