#include "api/Core.h"

#include "brep/bool/Context.h"
#include "brep/bool/IntersectPlanePlane.h"
#include "brep/Geometry.h"

#include <gtest/gtest.h>

#include <cmath>

namespace brep
{
namespace
{

TEST(IntersectPlanePlane, OrthogonalPlanesYieldLine)
{
  PlaneSurface xy{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};  // z = 0
  PlaneSurface xz{Point3d{0, 0, 0}, Vector3d{0, 1, 0}};  // y = 0

  const auto result = boolean::IntersectPlanePlane(xy, xz);
  ASSERT_TRUE(result.IsLine()) << result.Diagnostics;
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Line);

  // Direction ≈ ±X
  EXPECT_NEAR(std::abs(result.Direction.x()), 1.0, 1e-9);
  EXPECT_NEAR(result.Direction.y(), 0.0, 1e-9);
  EXPECT_NEAR(result.Direction.z(), 0.0, 1e-9);
  EXPECT_NEAR(result.Direction.norm(), 1.0, 1e-9);

  // Point lies on both planes
  EXPECT_NEAR(result.Point.z(), 0.0, 1e-9);
  EXPECT_NEAR(result.Point.y(), 0.0, 1e-9);
}

TEST(IntersectPlanePlane, ParallelDistinctNoIntersection)
{
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 1}, Vector3d{0, 0, 1}};

  const auto result = boolean::IntersectPlanePlane(a, b);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Parallel);
  EXPECT_FALSE(result.IsLine());
  EXPECT_FALSE(result.Diagnostics.empty());
}

TEST(IntersectPlanePlane, CoincidentExact)
{
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{1, 2, 0}, Vector3d{0, 0, 1}};

  const auto result = boolean::IntersectPlanePlane(a, b);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Coincident);
  EXPECT_FALSE(result.IsLine());
}

TEST(IntersectPlanePlane, CoincidentOppositeNormals)
{
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 0}, Vector3d{0, 0, -1}};

  const auto result = boolean::IntersectPlanePlane(a, b);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Coincident);
}

TEST(IntersectPlanePlane, CoincidentWithinFuzzy)
{
  boolean::BooleanContext ctx;
  ctx.fuzzy = 1e-6;
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 0.5e-6}, Vector3d{0, 0, 1}};

  const auto result = boolean::IntersectPlanePlane(a, b, ctx);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Coincident);
}

TEST(IntersectPlanePlane, BeyondFuzzyIsParallel)
{
  boolean::BooleanContext ctx;
  ctx.fuzzy = 1e-6;
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 1e-3}, Vector3d{0, 0, 1}};

  const auto result = boolean::IntersectPlanePlane(a, b, ctx);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Parallel);
}

TEST(IntersectPlanePlane, OriginNormalOverload)
{
  const auto result = boolean::IntersectPlanePlane(
      Point3d{0, 0, 0}, Vector3d{1, 0, 0}, Point3d{0, 0, 0}, Vector3d{0, 1, 0});
  ASSERT_TRUE(result.IsLine()) << result.Diagnostics;
  EXPECT_NEAR(std::abs(result.Direction.z()), 1.0, 1e-9);
  EXPECT_NEAR(result.Point.x(), 0.0, 1e-9);
  EXPECT_NEAR(result.Point.y(), 0.0, 1e-9);
}

}  // namespace
}  // namespace brep
