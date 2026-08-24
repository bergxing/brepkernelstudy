#include "api/Core.h"

#include "brep/bool/Context.h"
#include "brep/bool/IntersectPlaneSphere.h"
#include "brep/Geometry.h"

#include <gtest/gtest.h>

#include <cmath>

namespace brep
{
namespace
{

TEST(IntersectPlaneSphere, EquatorialPlaneYieldsUnitCircle)
{
  PlaneSurface xy{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  SphereSurface sphere{Point3d{0, 0, 0}, 1.0};

  const auto result = boolean::intersect_plane_sphere(xy, sphere);
  ASSERT_TRUE(result.is_circle()) << result.diagnostics;
  EXPECT_EQ(result.status, boolean::PlaneSphereStatus::Circle);
  EXPECT_NEAR(result.radius, 1.0, 1e-9);
  EXPECT_NEAR(result.center.x(), 0.0, 1e-9);
  EXPECT_NEAR(result.center.y(), 0.0, 1e-9);
  EXPECT_NEAR(result.center.z(), 0.0, 1e-9);
  EXPECT_NEAR(std::abs(result.normal.z()), 1.0, 1e-9);
  EXPECT_NEAR(result.normal.norm(), 1.0, 1e-9);
}

TEST(IntersectPlaneSphere, OffsetPlaneSmallerCircle)
{
  PlaneSurface z05{Point3d{0, 0, 0.5}, Vector3d{0, 0, 1}};
  SphereSurface sphere{Point3d{0, 0, 0}, 1.0};

  const auto result = boolean::intersect_plane_sphere(z05, sphere);
  ASSERT_TRUE(result.is_circle()) << result.diagnostics;
  EXPECT_NEAR(result.center.z(), 0.5, 1e-9);
  EXPECT_NEAR(result.radius, std::sqrt(1.0 - 0.25), 1e-9);
}

TEST(IntersectPlaneSphere, TangentYieldsPoint)
{
  PlaneSurface z1{Point3d{0, 0, 1}, Vector3d{0, 0, 1}};
  SphereSurface sphere{Point3d{0, 0, 0}, 1.0};

  const auto result = boolean::intersect_plane_sphere(z1, sphere);
  EXPECT_EQ(result.status, boolean::PlaneSphereStatus::Point);
  EXPECT_FALSE(result.is_circle());
  EXPECT_NEAR(result.center.x(), 0.0, 1e-9);
  EXPECT_NEAR(result.center.y(), 0.0, 1e-9);
  EXPECT_NEAR(result.center.z(), 1.0, 1e-9);
  EXPECT_NEAR(result.radius, 0.0, 1e-9);
}

TEST(IntersectPlaneSphere, MissYieldsEmpty)
{
  PlaneSurface z2{Point3d{0, 0, 2}, Vector3d{0, 0, 1}};
  SphereSurface sphere{Point3d{0, 0, 0}, 1.0};

  const auto result = boolean::intersect_plane_sphere(z2, sphere);
  EXPECT_EQ(result.status, boolean::PlaneSphereStatus::Empty);
  EXPECT_FALSE(result.diagnostics.empty());
}

TEST(IntersectPlaneSphere, TangentWithinFuzzy)
{
  boolean::BooleanContext ctx;
  ctx.fuzzy = 1e-6;
  PlaneSurface z{Point3d{0, 0, 1.0 + 0.5e-6}, Vector3d{0, 0, 1}};
  SphereSurface sphere{Point3d{0, 0, 0}, 1.0};

  const auto result = boolean::intersect_plane_sphere(z, sphere, ctx);
  EXPECT_EQ(result.status, boolean::PlaneSphereStatus::Point);
}

TEST(IntersectPlaneSphere, OriginNormalOverload)
{
  SphereSurface sphere{Point3d{0, 0, 0}, 2.0};
  const auto result = boolean::intersect_plane_sphere(
      Point3d{0, 0, 0}, Vector3d{1, 0, 0}, sphere.center(), sphere.radius());
  ASSERT_TRUE(result.is_circle());
  EXPECT_NEAR(result.radius, 2.0, 1e-9);
}

}  // namespace
}  // namespace brep
