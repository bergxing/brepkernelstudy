#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include "brep/bool/Boolean.h"
#include "brep/Geometry.h"

#include <gtest/gtest.h>

#include <cmath>

namespace brep
{
namespace
{

[[nodiscard]] bool point_in_triangle(const Point3d& p, const Point3d& a,
                                     const Point3d& b, const Point3d& c,
                                     double eps = 1e-9)
                                     {
  const Vector3d n = (b - a).cross(c - a);
  if (n.squaredNorm() < 1e-24) return false;
  const Vector3d na = (b - a).cross(p - a);
  const Vector3d nb = (c - b).cross(p - b);
  const Vector3d nc = (a - c).cross(p - c);
  return na.dot(n) >= -eps && nb.dot(n) >= -eps && nc.dot(n) >= -eps;
}

TEST(TessellateTrimmedSphere, SevenEighthsOmitsDeletedOctant)
{
  Model model;
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(box, nullptr);

  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});
  ASSERT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);

  const TriangleMesh mesh = tessellate_body(*result.body);
  ASSERT_FALSE(mesh.indices.empty());
  EXPECT_LT(mesh.vertices.size(), 300u)
      << "trimmed ⅞ sphere should be lighter than a full default sphere";

  // Point on the unit sphere inside the subtracted box (removed spherical cap).
  const Vector3d deleted_dir =
      Vector3d{0.5, 0.5, std::sqrt(0.5)}.normalized();
  const Point3d deleted{deleted_dir.x(), deleted_dir.y(), deleted_dir.z()};
  bool covered = false;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
  {
    const Point3d& a = mesh.vertices[mesh.indices[i]].position;
    const Point3d& b = mesh.vertices[mesh.indices[i + 1]].position;
    const Point3d& c = mesh.vertices[mesh.indices[i + 2]].position;
    const double ra = a.distance_to(Point3d{0, 0, 0});
    const double rb = b.distance_to(Point3d{0, 0, 0});
    const double rc = c.distance_to(Point3d{0, 0, 0});
    if (std::abs(ra - 1.0) > 0.05 || std::abs(rb - 1.0) > 0.05 ||
        std::abs(rc - 1.0) > 0.05)
    {
      continue;
    }
    if (point_in_triangle(deleted, a, b, c, 1e-6))
    {
      covered = true;
      break;
    }
  }
  EXPECT_FALSE(covered)
      << "deleted +++ octant sample must not lie in any triangle";

  // A point on the opposite octant should remain covered by the spherical face.
  const Vector3d kept_dir = Vector3d{-1, -1, -1}.normalized();
  const Point3d kept{kept_dir.x(), kept_dir.y(), kept_dir.z()};
  bool kept_covered = false;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
  {
    const Point3d& a = mesh.vertices[mesh.indices[i]].position;
    const Point3d& b = mesh.vertices[mesh.indices[i + 1]].position;
    const Point3d& c = mesh.vertices[mesh.indices[i + 2]].position;
    // Only consider triangles near the sphere (not the planar cut faces).
    const double ra = a.distance_to(Point3d{0, 0, 0});
    const double rb = b.distance_to(Point3d{0, 0, 0});
    const double rc = c.distance_to(Point3d{0, 0, 0});
    if (std::abs(ra - 1.0) > 0.05 || std::abs(rb - 1.0) > 0.05 ||
        std::abs(rc - 1.0) > 0.05)
    {
      continue;
    }
    if (point_in_triangle(kept, a, b, c, 1e-4))
    {
      kept_covered = true;
      break;
    }
  }
  EXPECT_TRUE(kept_covered)
      << "opposite octant on the sphere should still be tessellated";
}

}  // namespace
}  // namespace brep
