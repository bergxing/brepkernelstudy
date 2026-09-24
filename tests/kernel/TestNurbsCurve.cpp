#include "brep/feat/PrimitiveSpecs.h"

#include <gtest/gtest.h>

using namespace brep;

TEST(NurbsKnots, FourCvsIsBezierKnots)
{
    const auto u = ClampedUniformKnots(4, 3);
    ASSERT_EQ(u.size(), 8u);
    EXPECT_DOUBLE_EQ(u[0], 0.0);
    EXPECT_DOUBLE_EQ(u[3], 0.0);
    EXPECT_DOUBLE_EQ(u[4], 1.0);
    EXPECT_DOUBLE_EQ(u[7], 1.0);
}

TEST(NurbsKnots, FiveCvsHasMidKnot)
{
    const auto u = ClampedUniformKnots(5, 3);
    ASSERT_EQ(u.size(), 9u);
    EXPECT_DOUBLE_EQ(u[4], 0.5);
}

TEST(NurbsKnots, SixCvsSplitsThirds)
{
    const auto u = ClampedUniformKnots(6, 3);
    ASSERT_EQ(u.size(), 10u);
    EXPECT_NEAR(u[4], 1.0 / 3.0, 1e-12);
    EXPECT_NEAR(u[5], 2.0 / 3.0, 1e-12);
}

TEST(NurbsSpec, RejectsTooFewCvsBadWeightAndWrongDegree)
{
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{}, Point3d{1, 0, 0}, Point3d{2, 0, 0}};
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
    spec.Cvs.push_back(Point3d{3, 0, 0});
    EXPECT_TRUE(NurbsCurveSpecValid(spec));
    spec.Degree = 2;
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
    spec.Degree = 3;
    spec.Weights = {1, 1, 0, 1};
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
    spec.Weights = {1, 1, 2, 1};
    EXPECT_TRUE(NurbsCurveSpecValid(spec));
    spec.Knots = {0, 0, 0, 1};
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
}
