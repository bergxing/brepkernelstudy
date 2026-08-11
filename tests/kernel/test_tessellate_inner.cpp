#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/mesh.hpp"
#include "brep/ops/profile.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace brep {
namespace {

[[nodiscard]] bool point_in_triangle(const Point3d& p, const Point3d& a,
                                     const Point3d& b, const Point3d& c,
                                     double eps = 1e-9) {
  const Vector3d n = (b - a).cross(c - a);
  if (n.squaredNorm() < 1e-24) return false;
  const Vector3d na = (b - a).cross(p - a);
  const Vector3d nb = (c - b).cross(p - b);
  const Vector3d nc = (a - c).cross(p - c);
  return na.dot(n) >= -eps && nb.dot(n) >= -eps && nc.dot(n) >= -eps;
}

[[nodiscard]] double triangle_area(const Point3d& a, const Point3d& b,
                                   const Point3d& c) {
  return 0.5 * (b - a).cross(c - a).norm();
}

TEST(TessellateInner, HoleCenterNotCovered) {
  Model model;
  ops::ExtrudeSpec spec;
  spec.name = "plate_hole";
  spec.distance = 1.0;
  spec.plane = Plane::xy();
  spec.profile.outer = {
      Point2d{0, 0},
      Point2d{4, 0},
      Point2d{4, 4},
      Point2d{0, 4},
  };
  spec.profile.holes.push_back({
      Point2d{1, 1},
      Point2d{3, 1},
      Point2d{3, 3},
      Point2d{1, 3},
  });

  Body* body = ops::extrude(model, spec);
  ASSERT_NE(body, nullptr);

  const TriangleMesh mesh = tessellate_body(*body);
  ASSERT_FALSE(mesh.indices.empty());

  // Hole center on the top cap (z = 1).
  const Point3d hole_center{2, 2, 1};
  bool covered = false;
  double top_area = 0.0;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const Point3d& a = mesh.vertices[mesh.indices[i]].position;
    const Point3d& b = mesh.vertices[mesh.indices[i + 1]].position;
    const Point3d& c = mesh.vertices[mesh.indices[i + 2]].position;
    // Only consider triangles near the top plane z≈1.
    const double zavg = (a.z() + b.z() + c.z()) / 3.0;
    if (std::abs(zavg - 1.0) > 1e-6) continue;
    top_area += triangle_area(a, b, c);
    if (point_in_triangle(hole_center, a, b, c)) covered = true;
  }

  EXPECT_FALSE(covered) << "hole center should not lie in any top triangle";
  // Outer 16 − hole 4 = 12
  EXPECT_NEAR(top_area, 12.0, 0.25);
}

}  // namespace
}  // namespace brep
