#include "api/core.hpp"
#include "api/mesh.hpp"
#include "api/modeling.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace brep {
namespace {

TEST(TessellationOptions, ForRadiusDefaults) {
  const auto opts = TessellationOptions::for_radius(5.0);
  EXPECT_DOUBLE_EQ(opts.linear_deflection, 0.02 * 5.0);
  EXPECT_NEAR(opts.angular_deflection, 15.0 * std::numbers::pi / 180.0, 1e-12);
  EXPECT_EQ(opts.min_u_segments, 24);
  EXPECT_EQ(opts.min_v_segments, 12);
  EXPECT_EQ(opts.max_u_segments, 128);
  EXPECT_EQ(opts.max_v_segments, 64);

  const auto tiny = TessellationOptions::for_radius(1e-6);
  EXPECT_DOUBLE_EQ(tiny.linear_deflection, 1e-4);
}

TEST(TessellateSphere, DefaultMeshScaleAndNormals) {
  Model model;
  const Point3d center{1, -2, 3};
  const double radius = 2.0;
  Body* body = make_sphere(
      model, SphereSpec{.center = center, .radius = radius, .name = "s"});
  ASSERT_NE(body, nullptr);

  const TriangleMesh mesh = tessellate_body(*body);
  ASSERT_FALSE(mesh.vertices.empty());
  ASSERT_FALSE(mesh.indices.empty());
  EXPECT_EQ(mesh.indices.size() % 3, 0u);

  // Default angular 15° → nu ≥ 24; linear 0.02R → nv ≥ 12 (clamped to min).
  // Pole quads contribute one triangle after skipping degenerates.
  const std::size_t tri_count = mesh.indices.size() / 3;
  EXPECT_GE(tri_count, static_cast<std::size_t>(24 * 12));
  EXPECT_LE(tri_count, static_cast<std::size_t>(128 * 64 * 2));

  // CDT + interior UV lattice (not a rigid (nu+1)×(nv+1) grid).
  EXPECT_GE(mesh.vertices.size(), static_cast<std::size_t>(24 * 10));
  EXPECT_LE(mesh.vertices.size(), static_cast<std::size_t>(129 * 65));

  Face* face = body->shells.at(0)->faces.at(0);
  ASSERT_NE(face, nullptr);
  ASSERT_NE(face->surface, nullptr);
  ASSERT_EQ(face->surface->kind(), SurfaceKind::Sphere);
  const auto* sphere = static_cast<const SphereSurface*>(face->surface);

  // Sample mesh vertices: position on sphere; normal matches analytic.
  constexpr std::size_t kStride = 17;
  for (std::size_t i = 0; i < mesh.vertices.size(); i += kStride) {
    const MeshVertex& mv = mesh.vertices[i];
    EXPECT_NEAR((mv.position - center).norm(), radius, 1e-6);

    const Point2d uv = sphere->param_of(mv.position);
    const Vector3d expected = face->normal_at(uv.u(), uv.v());
    EXPECT_NEAR(mv.normal.norm(), 1.0, 1e-9);
    EXPECT_NEAR(mv.normal.dot(expected), 1.0, 1e-5)
        << "vertex " << i << " uv=(" << uv.u() << "," << uv.v() << ")";

    // Outward for solid sphere
    EXPECT_GT(mv.normal.dot(mv.position - center), 0.0);
  }

  // Find a non-degenerate triangle and check winding vs vertex normals.
  bool found_winding = false;
  for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
    const MeshVertex& a = mesh.vertices[mesh.indices[t]];
    const MeshVertex& b = mesh.vertices[mesh.indices[t + 1]];
    const MeshVertex& c = mesh.vertices[mesh.indices[t + 2]];
    const Vector3d geom =
        (b.position - a.position).cross(c.position - a.position);
    if (geom.squaredNorm() < 1e-18) continue;
    EXPECT_GT(geom.dot(a.normal), 0.0);
    found_winding = true;
    break;
  }
  EXPECT_TRUE(found_winding);
}

TEST(TessellateSphere, RespectsSegmentCaps) {
  Model model;
  Body* body =
      make_sphere(model, SphereSpec{.center = {0, 0, 0}, .radius = 1.0});
  ASSERT_NE(body, nullptr);

  TessellationOptions opts = TessellationOptions::for_radius(1.0);
  opts.min_u_segments = 8;
  opts.min_v_segments = 4;
  opts.max_u_segments = 10;
  opts.max_v_segments = 6;
  opts.angular_deflection = 1.0;  // would want few segments; caps still apply
  opts.linear_deflection = 0.5;

  const TriangleMesh mesh = tessellate_body(*body, opts);
  const std::size_t tri_count = mesh.indices.size() / 3;
  // At most max_u * max_v * 2; at least one triangle per min quad (poles degenerate).
  EXPECT_LE(tri_count, static_cast<std::size_t>(10 * 6 * 2));
  EXPECT_GE(tri_count, static_cast<std::size_t>(8 * 4));
}

TEST(TessellateBody, BoxStillFans) {
  Model model;
  Body* body = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}});
  ASSERT_NE(body, nullptr);
  const TriangleMesh mesh = tessellate_body(*body);
  EXPECT_EQ(mesh.indices.size() / 3, 12u);  // 6 faces × 2 tris
  EXPECT_FALSE(mesh.vertices.empty());
}

}  // namespace
}  // namespace brep
