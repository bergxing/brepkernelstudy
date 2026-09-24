#include "brep/Geometry.h"
#include "brep/feat/PrimitiveSpecs.h"

#include <gtest/gtest.h>

#include <vector>

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

// Hand calc (5 CV, knots [0,0,0,0,0.5,1,1,1,1], t=0.5): cubic basis
// N1=0.25, N2=0.5, N3=0.25 → unit weights give (2, 0.75, 0);
// with w2=2, y = 1.25/1.5.

TEST(NurbsCurve, FourUnitCvsMatchCubicBezier)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{0, 1, 0},
                                   Point3d{1, 1, 0}, Point3d{1, 0, 0}};
    const BezierCurve bezier(cvs);
    const NurbsCurve nurbs(cvs, {}, ClampedUniformKnots(4, 3));
    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        EXPECT_LT(nurbs.Eval(t).distance_to(bezier.Eval(t)), 1e-9) << t;
    }
    EXPECT_LT(nurbs.Eval(0).distance_to(cvs.front()), 1e-12);
    EXPECT_LT(nurbs.Eval(1).distance_to(cvs.back()), 1e-12);
}

TEST(NurbsCurve, FiveCvsAtMidIsNotOnControlPolygon)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{1, 0, 0},
                                   Point3d{2, 1, 0}, Point3d{3, 1, 0},
                                   Point3d{4, 0, 0}};
    const NurbsCurve curve(cvs, {}, ClampedUniformKnots(5, 3));
    const Point3d mid = curve.Eval(0.5);
    EXPECT_NEAR(mid.x(), 2.0, 1e-9);
    EXPECT_NEAR(mid.y(), 0.75, 1e-9);
    EXPECT_NEAR(mid.z(), 0.0, 1e-9);
    EXPECT_GT(mid.distance_to(Point3d{2, 1, 0}), 0.2);
}

TEST(NurbsCurve, HigherWeightPullsTowardThatCv)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{1, 0, 0},
                                   Point3d{2, 1, 0}, Point3d{3, 1, 0},
                                   Point3d{4, 0, 0}};
    const auto knots = ClampedUniformKnots(5, 3);
    const NurbsCurve unit(cvs, {}, knots);
    const NurbsCurve heavy(cvs, {1, 1, 2, 1, 1}, knots);
    EXPECT_GT(heavy.Eval(0.5).y(), unit.Eval(0.5).y());
    EXPECT_LT(heavy.Eval(0).distance_to(cvs.front()), 1e-12);
    EXPECT_LT(heavy.Eval(1).distance_to(cvs.back()), 1e-12);
    EXPECT_NEAR(heavy.Eval(0.5).y(), 1.25 / 1.5, 1e-9);
}

TEST(NurbsCurve, TangentAtZeroMatchesBezier)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{0, 1, 0},
                                   Point3d{1, 1, 0}, Point3d{1, 0, 0}};
    const BezierCurve bezier(cvs);
    const NurbsCurve nurbs(cvs, {}, ClampedUniformKnots(4, 3));
    const Vector3d a = bezier.Tangent(0.0);
    const Vector3d b = nurbs.Tangent(0.0);
    EXPECT_GT(a.dot(b), 0.99);
}

TEST(NurbsCurve, ApplyTransformMovesCvsKeepsKnotsAndWeights)
{
    const RigidTransform t{.Translation = Point3d{2, 3, 0}};
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{1, 0, 0},
                                   Point3d{2, 1, 0}, Point3d{3, 1, 0},
                                   Point3d{4, 0, 0}};
    const std::vector<double> weights{1, 1, 2, 1, 1};
    const std::vector<double> knots = ClampedUniformKnots(5, 3);
    NurbsCurve curve(cvs, weights, knots);
    ASSERT_EQ(curve.Knots().size(), 9u);

    ASSERT_TRUE(ApplyTransform(curve, t));
    EXPECT_NEAR(curve.Cvs()[0].x(), 2.0, 1e-12);
    EXPECT_NEAR(curve.Cvs()[0].y(), 3.0, 1e-12);
    EXPECT_NEAR(curve.Cvs()[4].x(), 6.0, 1e-12);
    ASSERT_EQ(curve.Weights().size(), 5u);
    EXPECT_NEAR(curve.Weights()[2], 2.0, 1e-12);
    ASSERT_EQ(curve.Knots().size(), 9u);
    EXPECT_DOUBLE_EQ(curve.Knots()[4], 0.5);
}
