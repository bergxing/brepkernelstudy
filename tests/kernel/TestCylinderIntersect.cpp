#include "api/Core.h"

#include "brep/bool/IntersectPlaneCylinder.h"
#include "brep/bool/IntersectSphereCylinder.h"
#include "brep/Geometry.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace brep
{
namespace
{

using std::numbers::pi;

TEST(IntersectPlaneCylinder, PerpendicularYieldsCircle)
{
  PlaneSurface xy{Point3d{0, 0, 0}, Vector3d{0, 0, 1}};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 2.0};
  const auto r = boolean::IntersectPlaneCylinder(xy, cyl);
  ASSERT_TRUE(r.IsCircle()) << r.Diagnostics;
  EXPECT_NEAR(r.MajorRadius, 2.0, 1e-9);
  EXPECT_NEAR(r.Center.z(), 0.0, 1e-9);
}

TEST(IntersectPlaneCylinder, ObliqueYieldsEllipse)
{
  PlaneSurface p{Point3d{0, 0, 0}, Vector3d{0, 1, 1}};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  const auto r = boolean::IntersectPlaneCylinder(p, cyl);
  EXPECT_EQ(r.status, boolean::PlaneCylinderStatus::Ellipse) << r.Diagnostics;
  EXPECT_NEAR(r.MinorRadius, 1.0, 1e-9);
  EXPECT_GT(r.MajorRadius, r.MinorRadius);
}

TEST(IntersectPlaneCylinder, ParallelTwoLines)
{
  PlaneSurface p{Point3d{0, 0, 0}, Vector3d{1, 0, 0}};  // x=0
  CylinderSurface cyl{Point3d{0.5, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  const auto r = boolean::IntersectPlaneCylinder(p, cyl);
  EXPECT_EQ(r.status, boolean::PlaneCylinderStatus::Lines) << r.Diagnostics;
  EXPECT_EQ(r.LinePoints.size(), 2u);
}

TEST(IntersectPlaneCylinder, ParallelMiss)
{
  PlaneSurface p{Point3d{3, 0, 0}, Vector3d{1, 0, 0}};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  EXPECT_EQ(boolean::IntersectPlaneCylinder(p, cyl).status,
            boolean::PlaneCylinderStatus::Empty);
}

TEST(IntersectSphereCylinder, CoaxialTwoCircles)
{
  SphereSurface sph{Point3d{0, 0, 0}, 2.0};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  const auto r = boolean::IntersectSphereCylinder(sph, cyl);
  ASSERT_TRUE(r.HasCircles()) << r.Diagnostics;
  ASSERT_EQ(r.Circles.size(), 2u);
  EXPECT_NEAR(r.Circles[0].Radius, 1.0, 1e-9);
  EXPECT_NEAR(std::abs(r.Circles[0].Center.z()), std::sqrt(3.0), 1e-9);
}

TEST(IntersectSphereCylinder, OffAxisUnsupported)
{
  SphereSurface sph{Point3d{2, 0, 0}, 1.0};
  CylinderSurface cyl{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, 1.0};
  EXPECT_EQ(boolean::IntersectSphereCylinder(sph, cyl).status,
            boolean::SphereCylinderStatus::Unsupported);
}

}  // namespace
}  // namespace brep
