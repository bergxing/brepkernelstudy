#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"
#include "brep/bool/Boolean.h"
#include "brep/io/XlDocument.h"
#include "brep/Part.h"
#include <gtest/gtest.h>
#include <cmath>
#include <array>
#include <numbers>
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
[[nodiscard]] bool mesh_covers_point(const TriangleMesh& mesh, const Point3d& p,
                                     double eps = 1e-6)
                                     {
  for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)
  {
    const Point3d& a = mesh.Vertices[mesh.Indices[i]].Position;
    const Point3d& b = mesh.Vertices[mesh.Indices[i + 1]].Position;
    const Point3d& c = mesh.Vertices[mesh.Indices[i + 2]].Position;
    if (point_in_triangle(p, a, b, c, eps)) return true;
  }
  return false;
}
[[nodiscard]] TriangleMesh tessellate_corner_union(
    const Point3d& box_min, const Point3d& box_max, const Point3d& center,
    double radius)
    {
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = box_min, .Max = box_max});
  Body* sphere = MakeSphere(model, SphereSpec{.Center = center, .Radius = radius});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *box, *sphere, {});
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  return TessellateBody(*result.OutputBody);
}
void expect_exterior_sphere_patch_covers(
    const TriangleMesh& mesh, const Point3d& center, double radius)
    {
  std::size_t misses = 0;
  std::size_t samples = 0;
  for (int iu = 0; iu <= 12; ++iu)
  {
    for (int iv = 1; iv <= 10; ++iv)
    {
      const double u = (2.0 * std::numbers::pi * iu) / 12.0;
      const double v =
          -0.5 * std::numbers::pi + (std::numbers::pi * iv) / 11.0;
      const Point3d p{center.x() + radius * std::cos(v) * std::cos(u),
                      center.y() + radius * std::cos(v) * std::sin(u),
                      center.z() + radius * std::sin(v)};
      const Vector3d dir = p - center;
      if (dir.x() <= 0 && dir.y() <= 0 && dir.z() >= 0) continue;
      ++samples;
      if (!mesh_covers_point(mesh, p, 2e-4)) ++misses;
    }
  }
  EXPECT_EQ(misses, 0u) << "sphere patch had " << misses << "/" << samples
                        << " uncovered samples";
}
void expect_north_pole_cap_covers(const TriangleMesh& mesh,
                                  const Point3d& center, double radius)
                                  {
  const Point3d north{center.x(), center.y(), center.z() + radius};
  std::size_t misses = 0;
  for (int iu = 0; iu <= 24; ++iu)
  {
    const double u = (2.0 * std::numbers::pi * iu) / 24.0;
    const double v = 0.5 * std::numbers::pi - 0.08;
    const Point3d p{center.x() + radius * std::cos(v) * std::cos(u),
                    center.y() + radius * std::cos(v) * std::sin(u),
                    center.z() + radius * std::sin(v)};
    const Vector3d dir = p - center;
    if (dir.x() <= 0 && dir.y() <= 0 && dir.z() >= 0) continue;
    if (!mesh_covers_point(mesh, p, 2e-4)) ++misses;
  }
  EXPECT_EQ(misses, 0u) << "north pole cap had " << misses << " uncovered samples";
  EXPECT_TRUE(mesh_covers_point(mesh, north, 2e-4));
}
void expect_near_trim_boundary_covers(const TriangleMesh& mesh,
                                      const Point3d& center, double radius)
                                      {
  std::size_t misses = 0;
  std::size_t samples = 0;
  // Sample just outside the deleted inward octant near the 3-arc trim.
  const std::array<Vector3d, 8> dirs =
  {
      Vector3d{1, 0, 0},  Vector3d{0, 1, 0},  Vector3d{0, 0, -1},
      Vector3d{1, 1, 0},  Vector3d{1, 0, -1}, Vector3d{0, 1, -1},
      Vector3d{1, 1, -1}, Vector3d{2, 1, -1},
  };
  for (const Vector3d& dir : dirs)
  {
    const Vector3d d = dir.normalized();
    if (d.x() <= 0 && d.y() <= 0 && d.z() >= 0) continue;
    const Point3d p{center.x() + radius * 0.98 * d.x(),
                    center.y() + radius * 0.98 * d.y(),
                    center.z() + radius * 0.98 * d.z()};
    ++samples;
    if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
  }
  EXPECT_EQ(misses, 0u) << "trim-boundary band had " << misses << "/"
                        << samples << " uncovered samples";
}
void expect_dense_complement_covers(const TriangleMesh& mesh,
                                    const Point3d& center, double radius)
                                    {
  std::size_t misses = 0;
  std::size_t samples = 0;
  for (int iu = 0; iu <= 24; ++iu)
  {
    for (int iv = 1; iv <= 12; ++iv)
    {
      const double u = (2.0 * std::numbers::pi * iu) / 24.0;
      const double v =
          -0.5 * std::numbers::pi + (std::numbers::pi * iv) / 13.0;
      const Point3d p{center.x() + radius * std::cos(v) * std::cos(u),
                      center.y() + radius * std::cos(v) * std::sin(u),
                      center.z() + radius * std::sin(v)};
      const Vector3d dir = p - center;
      // Deleted inward octant (same as boolean topology).
      if (dir.x() <= 0 && dir.y() <= 0 && dir.z() >= 0) continue;
      ++samples;
      if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
    }
  }
  EXPECT_EQ(misses, 0u) << "dense complement scan had " << misses << "/"
                        << samples << " uncovered samples";
}
void expect_arc_ribbon_covers(const TriangleMesh& mesh, const Point3d& center,
                              double radius)
                              {
  std::size_t misses = 0;
  std::size_t samples = 0;
  // Just outside each quarter-circle trim arc (sphere meets box face).
  const std::array<Vector3d, 12> offsets =
  {
      Vector3d{0.12, 0.02, 0.02},  Vector3d{0.02, 0.12, 0.02},
      Vector3d{0.02, 0.02, -0.12}, Vector3d{0.10, 0.10, 0.02},
      Vector3d{0.10, 0.02, -0.10}, Vector3d{0.02, 0.10, -0.10},
      Vector3d{0.08, 0.08, -0.08}, Vector3d{0.15, 0.05, 0.0},
      Vector3d{0.05, 0.15, 0.0},  Vector3d{0.05, 0.0, -0.15},
      Vector3d{0.06, 0.06, 0.06},  Vector3d{0.04, 0.04, -0.04},
  };
  for (const Vector3d& off : offsets)
  {
    const Vector3d base = Vector3d{-1, -1, 1}.normalized();
    const Vector3d d = (base + off).normalized();
    if (d.x() <= 0 && d.y() <= 0 && d.z() >= 0) continue;
    const Point3d p{center.x() + radius * 0.97 * d.x(),
                    center.y() + radius * 0.97 * d.y(),
                    center.z() + radius * 0.97 * d.z()};
    ++samples;
    if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
  }
  EXPECT_EQ(misses, 0u) << "arc ribbon had " << misses << "/" << samples
                        << " uncovered samples";
}
void expect_complement_outside_deleted_covers(const TriangleMesh& mesh,
                                              const Point3d& center,
                                              double radius)
                                              {
  std::size_t misses = 0;
  std::size_t samples = 0;
  // Just outside the deleted inward octant (should remain on the �?patch).
  const std::array<Vector3d, 10> dirs =
  {
      Vector3d{1, 0, 0},   Vector3d{0, 1, 0},   Vector3d{0, 0, -1},
      Vector3d{1, 1, 0},   Vector3d{1, 0, -1}, Vector3d{0, 1, -1},
      Vector3d{1, 1, -1},  Vector3d{0.5, 0.2, -0.3},
      Vector3d{0.2, 0.5, -0.3}, Vector3d{1, 0.2, -0.2},
  };
  for (const Vector3d& dir : dirs)
  {
    const Vector3d d = dir.normalized();
    if (d.x() <= 0 && d.y() <= 0 && d.z() >= 0) continue;
    const Point3d p{center.x() + radius * 0.96 * d.x(),
                    center.y() + radius * 0.96 * d.y(),
                    center.z() + radius * 0.96 * d.z()};
    ++samples;
    if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
  }
  EXPECT_EQ(misses, 0u) << "complement-outside-deleted had " << misses << "/"
                        << samples << " uncovered samples";
}
void expect_south_cap_wedge_covers(const TriangleMesh& mesh,
                                   const Point3d& center, double radius)
                                   {
  std::size_t misses = 0;
  std::size_t samples = 0;
  // Wedge seen in viewer: south cap sectors away from the deleted octant.
  for (int iu = 0; iu <= 24; ++iu)
  {
    for (int iv = 1; iv <= 6; ++iv)
    {
      const double u = (2.0 * std::numbers::pi * iu) / 24.0;
      const double v =
          -0.5 * std::numbers::pi + (std::numbers::pi * iv) / 28.0;
      const Point3d p{center.x() + radius * std::cos(v) * std::cos(u),
                      center.y() + radius * std::cos(v) * std::sin(u),
                      center.z() + radius * std::sin(v)};
      const Vector3d dir = p - center;
      if (dir.x() <= 0 && dir.y() <= 0 && dir.z() >= 0) continue;
      ++samples;
      if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
    }
  }
  EXPECT_EQ(misses, 0u) << "south-cap wedge had " << misses << "/" << samples
                        << " uncovered samples";
}
void expect_deleted_octant_wedge_covers(const TriangleMesh& mesh,
                                        const Point3d& center, double radius)
                                        {
  std::size_t misses = 0;
  std::size_t samples = 0;
  // Fan sectors adjacent to the deleted inward octant near the south cap.
  const std::array<Vector3d, 8> dirs =
  {
      Vector3d{0.15, 0.02, -0.98}, Vector3d{0.02, 0.15, -0.98},
      Vector3d{-0.15, 0.02, -0.98}, Vector3d{0.02, -0.15, -0.98},
      Vector3d{0.20, 0.20, -0.90}, Vector3d{0.25, 0.05, -0.85},
      Vector3d{0.05, 0.25, -0.85}, Vector3d{0.18, -0.18, -0.90},
  };
  for (const Vector3d& dir : dirs)
  {
    const Vector3d d = dir.normalized();
    if (d.x() <= 0 && d.y() <= 0 && d.z() >= 0) continue;
    const Point3d p{center.x() + radius * 0.94 * d.x(),
                    center.y() + radius * 0.94 * d.y(),
                    center.z() + radius * 0.94 * d.z()};
    ++samples;
    if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
  }
  EXPECT_EQ(misses, 0u) << "deleted-octant wedge had " << misses << "/"
                        << samples << " uncovered samples";
}
void expect_pole_caps_no_seams(const TriangleMesh& mesh, const Point3d& center,
                               double radius)
                               {
  std::size_t misses = 0;
  std::size_t samples = 0;
  for (int iu = 0; iu <= 32; ++iu)
  {
    for (int ring : {0, 1})
    {
      const double v =
          (ring == 0) ? -0.5 * std::numbers::pi + 0.06
                      : 0.5 * std::numbers::pi - 0.06;
      const double u = (2.0 * std::numbers::pi * iu) / 32.0;
      const Point3d p{center.x() + radius * std::cos(v) * std::cos(u),
                      center.y() + radius * std::cos(v) * std::sin(u),
                      center.z() + radius * std::sin(v)};
      const Vector3d dir = p - center;
      if (dir.x() <= 0 && dir.y() <= 0 && dir.z() >= 0) continue;
      ++samples;
      if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
    }
  }
  EXPECT_EQ(misses, 0u) << "pole-cap rings had " << misses << "/" << samples
                        << " uncovered samples";
}
void expect_far_from_corner_covers(const TriangleMesh& mesh,
                                   const Point3d& center, double radius)
                                   {
  std::size_t misses = 0;
  // Exterior patch far from the corner trim (opposite octant + north pole band).
  const std::array<Vector3d, 6> dirs =
  {
      Vector3d{-1, -1, -1}, Vector3d{-1, 0, -1}, Vector3d{0, -1, -1},
      Vector3d{-1, -1, 0},  Vector3d{0, 0, 1},   Vector3d{-0.5, -0.5, 1},
  };
  for (const Vector3d& dir : dirs)
  {
    const Vector3d d = dir.normalized();
    const Point3d p{center.x() + radius * 0.95 * d.x(),
                    center.y() + radius * 0.95 * d.y(),
                    center.z() + radius * 0.95 * d.z()};
    if (!mesh_covers_point(mesh, p, 3e-4)) ++misses;
  }
  EXPECT_EQ(misses, 0u) << "far-from-union sphere samples had " << misses
                        << " uncovered points";
}
TEST(TessellateCornerUnion, UntitledXlSecondCornerHasNoHoles)
{
  // Geometry from untitled.xl bool_sphere_box_Union (fa1a0d73�?.
  const Point3d box_min{-2.944, 0, 1.68354};
  const Point3d box_max{-2.13126, 0.826297, 2.35774};
  const Point3d center = box_max;
  const double radius = 0.337101;
  const TriangleMesh mesh =
      tessellate_corner_union(box_min, box_max, center, radius);
  ASSERT_FALSE(mesh.Indices.empty());
  // Sphere patch alone should be hundreds of tris, not the ~30 from UV-CDT.
  EXPECT_GT(mesh.Indices.size() / 3, 400u);
  const Point3d box_top_far{(box_min.x() + box_max.x()) * 0.5,
                            box_max.y() * 0.5, box_max.z()};
  EXPECT_TRUE(mesh_covers_point(mesh, box_top_far, 2e-4));
  expect_exterior_sphere_patch_covers(mesh, center, radius);
  expect_north_pole_cap_covers(mesh, center, radius);
  expect_near_trim_boundary_covers(mesh, center, radius);
  expect_arc_ribbon_covers(mesh, center, radius);
  expect_dense_complement_covers(mesh, center, radius);
  expect_pole_caps_no_seams(mesh, center, radius);
  expect_south_cap_wedge_covers(mesh, center, radius);
  expect_complement_outside_deleted_covers(mesh, center, radius);
  expect_deleted_octant_wedge_covers(mesh, center, radius);
  expect_far_from_corner_covers(mesh, center, radius);
}
TEST(TessellateCornerUnion, UntitledXlUnionGuidHasNoSphereHoles)
{
  const char* xl_path = "C:/Users/xingbl/Desktop/untitled.xl";
  const auto loaded = io::LoadXl(xl_path);
  if (!loaded.Ok())
  {
    GTEST_SKIP() << "untitled.xl not available: " << loaded.Error;
  }
  Part* part = loaded.document->MainPart();
  ASSERT_NE(part, nullptr);
  Body* body = nullptr;
  for (const char* guid_text :
       {"b724e248-7ccf-4a47-bad4-fce095d049c4",
        "323f39af-72eb-45ff-b146-939a47ad7271",
        "179ec7f0-4f06-474c-aa0d-83a1738b4cc8",
        "954e3b4a-2c31-416e-b12c-63ef7bd11e3a",
        "96c98914-d3f9-47bb-a056-e63da9c5a273",
        "fa1a0d73-a710-43b3-9d78-1d268d49d69d"})
        {
    body = part->FindBody(Guid::FromString(guid_text));
    if (body) break;
  }
  if (!body)
  {
    GTEST_SKIP() << "bool_sphere_box_Union guid not in xl";
  }
  Point3d center;
  double radius = 0.0;
  bool have_sphere = false;
  for (Shell* shell : body->Shells)
  {
    if (!shell) continue;
    for (Face* face : shell->Faces)
    {
      if (!face || !face->Surface ||
          face->Surface->Kind() != SurfaceKind::Sphere)
          {
        continue;
      }
      if (const auto* s = dynamic_cast<const SphereSurface*>(face->Surface))
      {
        center = s->Center();
        radius = s->Radius();
        have_sphere = true;
      }
    }
  }
  ASSERT_TRUE(have_sphere);
  ASSERT_GT(radius, 0.0);
  const TriangleMesh mesh = TessellateBody(*body);
  ASSERT_FALSE(mesh.Indices.empty());
  EXPECT_GT(mesh.Indices.size() / 3, 400u);
  expect_exterior_sphere_patch_covers(mesh, center, radius);
  expect_north_pole_cap_covers(mesh, center, radius);
  expect_near_trim_boundary_covers(mesh, center, radius);
  expect_arc_ribbon_covers(mesh, center, radius);
  expect_dense_complement_covers(mesh, center, radius);
  expect_pole_caps_no_seams(mesh, center, radius);
  expect_south_cap_wedge_covers(mesh, center, radius);
  expect_complement_outside_deleted_covers(mesh, center, radius);
  expect_deleted_octant_wedge_covers(mesh, center, radius);
  expect_far_from_corner_covers(mesh, center, radius);
}
}  // namespace
}  // namespace brep
