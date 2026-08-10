#include "api/core.hpp"

#include "brep/bool/context.hpp"
#include "brep/bool/intersect_plane_plane.hpp"
#include "brep/geometry.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace brep {
namespace {

TEST(IntersectPlanePlane, OrthogonalPlanesYieldLine) {
  PlaneSurface xy{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};  // z = 0
  PlaneSurface xz{Point3d{0, 0, 0}, Vector3d{0, 1, 0}};  // y = 0

  const auto result = boolean::intersect_plane_plane(xy, xz);
  ASSERT_TRUE(result.is_line()) << result.diagnostics;
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Line);

  // Direction ≈ ±X
  EXPECT_NEAR(std::abs(result.direction.x()), 1.0, 1e-9);
  EXPECT_NEAR(result.direction.y(), 0.0, 1e-9);
  EXPECT_NEAR(result.direction.z(), 0.0, 1e-9);
  EXPECT_NEAR(result.direction.norm(), 1.0, 1e-9);

  // Point lies on both planes
  EXPECT_NEAR(result.point.z(), 0.0, 1e-9);
  EXPECT_NEAR(result.point.y(), 0.0, 1e-9);
}

TEST(IntersectPlanePlane, ParallelDistinctNoIntersection) {
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 1}, Vector3d{0, 0, 1}};

  const auto result = boolean::intersect_plane_plane(a, b);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Parallel);
  EXPECT_FALSE(result.is_line());
  EXPECT_FALSE(result.diagnostics.empty());
}

TEST(IntersectPlanePlane, CoincidentExact) {
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{1, 2, 0}, Vector3d{0, 0, 1}};

  const auto result = boolean::intersect_plane_plane(a, b);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Coincident);
  EXPECT_FALSE(result.is_line());
}

TEST(IntersectPlanePlane, CoincidentOppositeNormals) {
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 0}, Vector3d{0, 0, -1}};

  const auto result = boolean::intersect_plane_plane(a, b);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Coincident);
}

TEST(IntersectPlanePlane, CoincidentWithinFuzzy) {
  boolean::BooleanContext ctx;
  ctx.fuzzy = 1e-6;
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 0.5e-6}, Vector3d{0, 0, 1}};

  const auto result = boolean::intersect_plane_plane(a, b, ctx);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Coincident);
}

TEST(IntersectPlanePlane, BeyondFuzzyIsParallel) {
  boolean::BooleanContext ctx;
  ctx.fuzzy = 1e-6;
  PlaneSurface a{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  PlaneSurface b{Point3d{0, 0, 1e-3}, Vector3d{0, 0, 1}};

  const auto result = boolean::intersect_plane_plane(a, b, ctx);
  EXPECT_EQ(result.status, boolean::PlanePlaneStatus::Parallel);
}

TEST(IntersectPlanePlane, OriginNormalOverload) {
  const auto result = boolean::intersect_plane_plane(
      Point3d{0, 0, 0}, Vector3d{1, 0, 0}, Point3d{0, 0, 0}, Vector3d{0, 1, 0});
  ASSERT_TRUE(result.is_line()) << result.diagnostics;
  EXPECT_NEAR(std::abs(result.direction.z()), 1.0, 1e-9);
  EXPECT_NEAR(result.point.x(), 0.0, 1e-9);
  EXPECT_NEAR(result.point.y(), 0.0, 1e-9);
}

}  // namespace
}  // namespace brep
