#include "api/Core.h"
#include "api/Modeling.h"

#include <gtest/gtest.h>

#include <cmath>

namespace brep
{
namespace
{

TEST(BezierCurve, Endpoints)
{
    const BezierCurve curve(Point3d{0, 0, 0}, Point3d{0, 1, 0},
                            Point3d{1, 1, 0}, Point3d{1, 0, 0});
    EXPECT_NEAR(curve.Eval(0.0).x(), 0.0, 1e-12);
    EXPECT_NEAR(curve.Eval(0.0).y(), 0.0, 1e-12);
    EXPECT_NEAR(curve.Eval(1.0).x(), 1.0, 1e-12);
    EXPECT_NEAR(curve.Eval(1.0).y(), 0.0, 1e-12);
    EXPECT_EQ(curve.Kind(), CurveKind::Bezier);
    EXPECT_NEAR(curve.Domain().first, 0.0, 1e-15);
    EXPECT_NEAR(curve.Domain().second, 1.0, 1e-15);
}

TEST(BezierCurve, MidpointMatchesBernstein)
{
    // P0=(0,0,0), P1=(0,1,0), P2=(1,1,0), P3=(1,0,0), t=0.5
    // B = 0.125*P0 + 0.375*P1 + 0.375*P2 + 0.125*P3 = (0.5, 0.75, 0)
    const BezierCurve curve(Point3d{0, 0, 0}, Point3d{0, 1, 0},
                            Point3d{1, 1, 0}, Point3d{1, 0, 0});
    const Point3d mid = curve.Eval(0.5);
    EXPECT_NEAR(mid.x(), 0.5, 1e-12);
    EXPECT_NEAR(mid.y(), 0.75, 1e-12);
    EXPECT_NEAR(mid.z(), 0.0, 1e-12);
}

TEST(BezierCurve, CollinearStaysOnSegment)
{
    const Point3d p0{0, 0, 0};
    const Point3d p3{4, 0, 0};
    const BezierCurve curve(p0, Point3d{1, 0, 0}, Point3d{3, 0, 0}, p3);
    const Vector3d chord = p3 - p0;
    for (double t = 0.0; t <= 1.0 + 1e-9; t += 0.1)
    {
        const Point3d p = curve.Eval(t);
        const Vector3d d = p - p0;
        EXPECT_NEAR(d.cross(chord).norm(), 0.0, 1e-9);
    }
}

TEST(BezierCurve, TangentAtEnds)
{
    const BezierCurve curve(Point3d{0, 0, 0}, Point3d{0, 2, 0},
                            Point3d{2, 2, 0}, Point3d{2, 0, 0});
    const Vector3d t0 = curve.Tangent(0.0);
    const Vector3d t1 = curve.Tangent(1.0);
    EXPECT_GT(t0.dot(Vector3d{0, 1, 0}), 0.9);
    EXPECT_GT(t1.dot(Vector3d{0, -1, 0}), 0.9);
}

TEST(BezierCurve, DegenerateTangentFallback)
{
    const BezierCurve curve(Point3d{0, 0, 0}, Point3d{0, 0, 0},
                            Point3d{1, 0, 0}, Point3d{1, 0, 0});
    const Vector3d t = curve.Tangent(0.0);
    EXPECT_NEAR(t.norm(), 1.0, 1e-12);
    EXPECT_FALSE(std::isnan(t.x()));
}

TEST(BezierCurve, SamplePolyline)
{
    const BezierCurve curve(Point3d{0, 0, 0}, Point3d{0, 1, 0},
                            Point3d{1, 1, 0}, Point3d{1, 0, 0});
    const auto pts = SampleBezierPolyline(curve, 32);
    ASSERT_EQ(pts.size(), 33u);
    EXPECT_NEAR(pts.front().x(), 0.0, 1e-12);
    EXPECT_NEAR(pts.back().x(), 1.0, 1e-12);
    for (std::size_t i = 1; i < pts.size(); ++i)
    {
        EXPECT_GT((pts[i] - pts[i - 1]).norm(), 0.0);
    }
}

TEST(BezierCurve, MakeBezierOwnedByModel)
{
    Model model;
    BezierCurve* curve =
        model.MakeBezier(Point3d{0, 0, 0}, Point3d{0, 1, 0}, Point3d{1, 1, 0},
                         Point3d{1, 0, 0});
    ASSERT_NE(curve, nullptr);
    EXPECT_EQ(curve->Kind(), CurveKind::Bezier);
    EXPECT_NEAR(curve->Eval(0.5).y(), 0.75, 1e-12);
}

TEST(BezierCurve, LineTwoCvs)
{
    const BezierCurve curve(std::vector<Point3d>{{0, 0, 0}, {4, 0, 0}});
    EXPECT_EQ(curve.Degree(), 1);
    const Point3d mid = curve.Eval(0.5);
    EXPECT_NEAR(mid.x(), 2.0, 1e-12);
    EXPECT_NEAR(mid.y(), 0.0, 1e-12);
}

TEST(BezierCurve, QuadraticParabola)
{
    const BezierCurve curve(
        std::vector<Point3d>{{0, 0, 0}, {1, 2, 0}, {2, 0, 0}});
    EXPECT_EQ(curve.Degree(), 2);
    const Point3d mid = curve.Eval(0.5);
    EXPECT_NEAR(mid.x(), 1.0, 1e-12);
    EXPECT_NEAR(mid.y(), 1.0, 1e-12);
}

TEST(BezierCurve, RationalQuarterCircle)
{
    const double w1 = std::sqrt(2.0) * 0.5;
    const BezierCurve curve(std::vector<Point3d>{{1, 0, 0}, {1, 1, 0}, {0, 1, 0}},
                            std::vector<double>{1.0, w1, 1.0});
    for (double t = 0.0; t <= 1.0 + 1e-9; t += 0.125)
    {
        const Point3d p = curve.Eval(t);
        EXPECT_NEAR(p.x() * p.x() + p.y() * p.y(), 1.0, 1e-9) << t;
        EXPECT_NEAR(p.z(), 0.0, 1e-12);
    }
    const Point3d mid = curve.Eval(0.5);
    EXPECT_NEAR(mid.x(), w1, 1e-9);
    EXPECT_NEAR(mid.y(), w1, 1e-9);
}

TEST(ApplyTransform, TranslatesBezierSpec)
{
    RigidTransform t{.Translation = Point3d{1, 2, 3}};
    auto out = ApplyTransform(
        PrimitiveSpec{BezierSpec{.Cvs = {{0, 0, 0},
                                         {0, 1, 0},
                                         {1, 1, 0},
                                         {1, 0, 0}}}},
        t);
    ASSERT_TRUE(out.has_value());
    const auto* bezier = std::get_if<BezierSpec>(&*out);
    ASSERT_NE(bezier, nullptr);
    EXPECT_NEAR(bezier->P0().x(), 1.0, 1e-12);
    EXPECT_NEAR(bezier->P0().y(), 2.0, 1e-12);
    EXPECT_NEAR(bezier->P3().z(), 3.0, 1e-12);
}

}  // namespace
}  // namespace brep
