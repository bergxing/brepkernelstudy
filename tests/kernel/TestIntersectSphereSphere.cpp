#include "api/Core.h"

#include "brep/bool/IntersectSphereSphere.h"
#include "brep/Geometry.h"

#include <gtest/gtest.h>

#include <cmath>

namespace brep
{
namespace
{

TEST(IntersectSphereSphere, EqualOverlapYieldsCircle)
{
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{1, 0, 0}, 1.0};
  const auto r = boolean::IntersectSphereSphere(a, b);
  ASSERT_TRUE(r.IsCircle()) << r.Diagnostics;
  EXPECT_NEAR(r.Center.x(), 0.5, 1e-9);
  EXPECT_NEAR(r.Radius, std::sqrt(0.75), 1e-9);
  EXPECT_NEAR(std::abs(r.Normal.x()), 1.0, 1e-9);
}

TEST(IntersectSphereSphere, Separate)
{
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{3, 0, 0}, 1.0};
  EXPECT_EQ(boolean::IntersectSphereSphere(a, b).status,
            boolean::SphereSphereStatus::Separate);
}

TEST(IntersectSphereSphere, ExternalTangent)
{
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{2, 0, 0}, 1.0};
  const auto r = boolean::IntersectSphereSphere(a, b);
  EXPECT_EQ(r.status, boolean::SphereSphereStatus::Point);
  EXPECT_NEAR(r.Center.x(), 1.0, 1e-9);
}

TEST(IntersectSphereSphere, Contained)
{
  SphereSurface a{Point3d{0, 0, 0}, 2.0};
  SphereSurface b{Point3d{0.5, 0, 0}, 0.5};
  EXPECT_EQ(boolean::IntersectSphereSphere(a, b).status,
            boolean::SphereSphereStatus::Contained);
}

TEST(IntersectSphereSphere, Coincident)
{
  SphereSurface a{Point3d{0, 0, 0}, 1.0};
  SphereSurface b{Point3d{0, 0, 0}, 1.0};
  EXPECT_EQ(boolean::IntersectSphereSphere(a, b).status,
            boolean::SphereSphereStatus::Coincident);
}

}  // namespace
}  // namespace brep
