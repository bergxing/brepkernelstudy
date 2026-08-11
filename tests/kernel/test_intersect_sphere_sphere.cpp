#include "api/core.hpp"

#include "brep/bool/intersect_sphere_sphere.hpp"
#include "brep/geometry.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace brep {
namespace {

TEST(IntersectSphereSphere, EqualOverlapYieldsCircle) {
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{1, 0, 0}, 1.0};
  const auto r = boolean::intersect_sphere_sphere(a, b);
  ASSERT_TRUE(r.is_circle()) << r.diagnostics;
  EXPECT_NEAR(r.center.x(), 0.5, 1e-9);
  EXPECT_NEAR(r.radius, std::sqrt(0.75), 1e-9);
  EXPECT_NEAR(std::abs(r.normal.x()), 1.0, 1e-9);
}

TEST(IntersectSphereSphere, Separate) {
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{3, 0, 0}, 1.0};
  EXPECT_EQ(boolean::intersect_sphere_sphere(a, b).status,
            boolean::SphereSphereStatus::Separate);
}

TEST(IntersectSphereSphere, ExternalTangent) {
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{2, 0, 0}, 1.0};
  const auto r = boolean::intersect_sphere_sphere(a, b);
  EXPECT_EQ(r.status, boolean::SphereSphereStatus::Point);
  EXPECT_NEAR(r.center.x(), 1.0, 1e-9);
}

TEST(IntersectSphereSphere, Contained) {
  SphereSurface a{Point3d{0, 0, 0}, 2.0};
  SphereSurface b{Point3d{0.5, 0, 0}, 0.5};
  EXPECT_EQ(boolean::intersect_sphere_sphere(a, b).status,
            boolean::SphereSphereStatus::Contained);
}

TEST(IntersectSphereSphere, Coincident) {
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{0, 0, 0}, 1.0};
  EXPECT_EQ(boolean::intersect_sphere_sphere(a, b).status,
            boolean::SphereSphereStatus::Coincident);
}

}  // namespace
}  // namespace brep
