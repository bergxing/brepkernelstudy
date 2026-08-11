#include "api/core.hpp"

#include "brep/bool/intersect_plane_cylinder.hpp"
#include "brep/bool/intersect_sphere_cylinder.hpp"
#include "brep/geometry.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace brep {
namespace {

using std::numbers::pi;

TEST(IntersectPlaneCylinder, PerpendicularYieldsCircle) {
  PlaneSurface xy{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 2.0};
  const auto r = boolean::intersect_plane_cylinder(xy, cyl);
  ASSERT_TRUE(r.is_circle()) << r.diagnostics;
  EXPECT_NEAR(r.major_radius, 2.0, 1e-9);
  EXPECT_NEAR(r.center.z(), 0.0, 1e-9);
}

TEST(IntersectPlaneCylinder, ObliqueYieldsEllipse) {
  PlaneSurface p{Point3d{0, 0, 0}, Vector3d{0, 1, 1}};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  const auto r = boolean::intersect_plane_cylinder(p, cyl);
  EXPECT_EQ(r.status, boolean::PlaneCylinderStatus::Ellipse) << r.diagnostics;
  EXPECT_NEAR(r.minor_radius, 1.0, 1e-9);
  EXPECT_GT(r.major_radius, r.minor_radius);
}

TEST(IntersectPlaneCylinder, ParallelTwoLines) {
  PlaneSurface p{Point3d{0, 0, 0}, Vector3d{1, 0, 0}};  // x=0
  CylinderSurface cyl{Point3d{0.5, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  const auto r = boolean::intersect_plane_cylinder(p, cyl);
  EXPECT_EQ(r.status, boolean::PlaneCylinderStatus::Lines) << r.diagnostics;
  EXPECT_EQ(r.line_points.size(), 2u);
}

TEST(IntersectPlaneCylinder, ParallelMiss) {
  PlaneSurface p{Point3d{3, 0, 0}, Vector3d{1, 0, 0}};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  EXPECT_EQ(boolean::intersect_plane_cylinder(p, cyl).status,
            boolean::PlaneCylinderStatus::Empty);
}

TEST(IntersectSphereCylinder, CoaxialTwoCircles) {
  SphereSurface sph{Point3d{0, 0, 0}, 2.0};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  const auto r = boolean::intersect_sphere_cylinder(sph, cyl);
  ASSERT_TRUE(r.has_circles()) << r.diagnostics;
  ASSERT_EQ(r.circles.size(), 2u);
  EXPECT_NEAR(r.circles[0].radius, 1.0, 1e-9);
  EXPECT_NEAR(std::abs(r.circles[0].center.z()), std::sqrt(3.0), 1e-9);
}

TEST(IntersectSphereCylinder, OffAxisUnsupported) {
  SphereSurface sph{Point3d{2, 0, 0}, 1.0};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  EXPECT_EQ(boolean::intersect_sphere_cylinder(sph, cyl).status,
            boolean::SphereCylinderStatus::Unsupported);
}

}  // namespace
}  // namespace brep
