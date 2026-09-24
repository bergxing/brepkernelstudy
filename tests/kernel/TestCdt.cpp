#include "brep/mesh/Cdt.h"

#include <gtest/gtest.h>

#include <cmath>

using brep::Point2d;
using brep::mesh::triangulate_constrained;

namespace
{

[[nodiscard]] bool point_in_triangle(const Point2d& p, const Point2d& a,
                                     const Point2d& b, const Point2d& c,
                                     double eps = 1e-12)
                                     {
  const double denominator =
      (b.v() - c.v()) * (a.u() - c.u()) +
      (c.u() - b.u()) * (a.v() - c.v());
  if (std::abs(denominator) <= eps)
  {
    return false;
  }
  const double alpha =
      ((b.v() - c.v()) * (p.u() - c.u()) +
       (c.u() - b.u()) * (p.v() - c.v())) /
      denominator;
  const double beta =
      ((c.v() - a.v()) * (p.u() - c.u()) +
       (a.u() - c.u()) * (p.v() - c.v())) /
      denominator;
  const double gamma = 1.0 - alpha - beta;
  return alpha >= -eps && beta >= -eps && gamma >= -eps;
}

}  // namespace

TEST(Cdt, UnconstrainedSquareTwoTriangles)
{
  std::vector<Point2d> pts = {
      {0, 0}, {1, 0}, {1, 1}, {0, 1},
  };
  const auto r = triangulate_constrained(pts, {});
  ASSERT_TRUE(r.Ok) << r.Diagnostics;
  EXPECT_EQ(r.Triangles.size(), 2u);
  // All vertices referenced in range
  for (const auto& t : r.Triangles)
  {
    for (int k = 0; k < 3; ++k)
  {
      EXPECT_GE(t.v[k], 0);
      EXPECT_LT(t.v[k], static_cast<int>(r.Vertices.size()));
    }
  }
}

TEST(Cdt, SquareWithHoleKeepsBoundaryAndDropsInterior)
{
  const std::vector<Point2d> outer = {
      {0, 0}, {4, 0}, {4, 4}, {0, 4},
  };
  const std::vector<Point2d> hole = {
      {1, 1}, {1, 3}, {3, 3}, {3, 1},
  };
  const auto r =
      brep::mesh::triangulate_polygon_with_holes(outer, {hole});
  ASSERT_TRUE(r.Ok) << r.Diagnostics;
  ASSERT_FALSE(r.Triangles.empty());

  const auto tri_contains = [&](const Point2d& point)
  {
    for (const auto& triangle : r.Triangles)
  {
      const Point2d& a = r.Vertices[triangle.v[0]].Uv;
      const Point2d& b = r.Vertices[triangle.v[1]].Uv;
      const Point2d& c = r.Vertices[triangle.v[2]].Uv;
      if (point_in_triangle(point, a, b, c))
      {
        return true;
      }
    }
    return false;
  };

  EXPECT_FALSE(tri_contains({2, 2}));
  EXPECT_TRUE(tri_contains({0.5, 0.5}));
}
