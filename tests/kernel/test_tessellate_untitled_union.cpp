#include "api/core.hpp"
#include "api/mesh.hpp"
#include "api/modeling.hpp"

#include "brep/bool/boolean.hpp"
#include "brep/validate.hpp"

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

[[nodiscard]] bool mesh_covers_point(const TriangleMesh& mesh,
                                     const Point3d& p, double eps = 1e-6) {
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const Point3d& a = mesh.vertices[mesh.indices[i]].position;
    const Point3d& b = mesh.vertices[mesh.indices[i + 1]].position;
    const Point3d& c = mesh.vertices[mesh.indices[i + 2]].position;
    if (point_in_triangle(p, a, b, c, eps)) return true;
  }
  return false;
}

TEST(TessellateUntitledUnion, CornerSphereBoxLooksTrimmed) {
  // Bodies from untitled.xl (box_copy_copy + sphere at max/max/min corner).
  Model model;
  Body* box = make_box(
      model, BoxSpec{.min = {-2.38421, 0, 4.59474},
                     .max = {-1.57147, 0.826297, 5.26894},
                     .name = "box_copy_copy"});
  Body* sphere = make_sphere(
      model, SphereSpec{.center = {-1.57147, 0.826297, 4.59474},
                        .radius = 0.413149,
                        .name = "sphere"});
  ASSERT_NE(box, nullptr);
  ASSERT_NE(sphere, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *box, *sphere, {});
  ASSERT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  ASSERT_TRUE(validate_body(*result.body).ok());

  const TriangleMesh mesh = tessellate_body(*result.body);
  ASSERT_FALSE(mesh.indices.empty());

  // Prior broken display tessellated a near-full sphere (~325 verts) plus bad
  // planes; trimmed union should stay well below that.
  EXPECT_LT(mesh.vertices.size(), 300u);

  // Box top face away from the corner hole should be covered.
  const Point3d box_top_center{
      (-2.38421 + -1.57147) * 0.5,
      (0 + 0.826297) * 0.5,
      5.26894};
  EXPECT_TRUE(mesh_covers_point(mesh, box_top_center, 1e-4))
      << "box top away from corner should be tessellated";

  // Inward octant on the sphere (sx=-1, sy=-1, sz=+1) is interior to the
  // union — must not appear as exterior spherical mesh.
  const Point3d center{-1.57147, 0.826297, 4.59474};
  const double r = 0.413149;
  const Vector3d inward_dir = Vector3d{-1, -1, 1}.normalized();
  const Point3d inward_on_sphere{center.x() + r * inward_dir.x(),
                                 center.y() + r * inward_dir.y(),
                                 center.z() + r * inward_dir.z()};
  bool inward_covered_by_sphere = false;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const std::uint32_t ia = mesh.indices[i];
    const std::uint32_t ib = mesh.indices[i + 1];
    const std::uint32_t ic = mesh.indices[i + 2];
    const Point3d& a = mesh.vertices[ia].position;
    const Point3d& b = mesh.vertices[ib].position;
    const Point3d& c = mesh.vertices[ic].position;
    const Vector3d radial = (inward_on_sphere - center).normalized();
    if (mesh.vertices[ia].normal.dot(radial) < 0.85 ||
        mesh.vertices[ib].normal.dot(radial) < 0.85 ||
        mesh.vertices[ic].normal.dot(radial) < 0.85) {
      continue;
    }
    if (point_in_triangle(inward_on_sphere, a, b, c, 1e-4)) {
      inward_covered_by_sphere = true;
      break;
    }
  }
  EXPECT_FALSE(inward_covered_by_sphere)
      << "inward spherical octant must not appear on exterior sphere mesh";

  // Exterior spherical patch opposite the corner should remain covered.
  const Vector3d outward_dir = Vector3d{1, 1, -1}.normalized();
  const Point3d outward_on_sphere{center.x() + r * outward_dir.x(),
                                    center.y() + r * outward_dir.y(),
                                    center.z() + r * outward_dir.z()};
  bool outward_covered = false;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
    const Point3d& a = mesh.vertices[mesh.indices[i]].position;
    const Point3d& b = mesh.vertices[mesh.indices[i + 1]].position;
    const Point3d& c = mesh.vertices[mesh.indices[i + 2]].position;
    const double ra = a.distance_to(center);
    const double rb = b.distance_to(center);
    const double rc = c.distance_to(center);
    if (std::abs(ra - r) > 0.02 || std::abs(rb - r) > 0.02 ||
        std::abs(rc - r) > 0.02) {
      continue;
    }
    if (point_in_triangle(outward_on_sphere, a, b, c, 1e-4)) {
      outward_covered = true;
      break;
    }
  }
  EXPECT_TRUE(outward_covered)
      << "exterior spherical cap should be tessellated";
}

}  // namespace
}  // namespace brep
