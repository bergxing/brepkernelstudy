#include "api/Core.h"
#include "api/Modeling.h"

#include <gtest/gtest.h>

#include <variant>

namespace brep
{
namespace
{

TEST(ApplyTransform, TranslatesPointLinePlaneSphere)
{
    const RigidTransform t{.Translation = Point3d{2, 3, 4}};
    ASSERT_TRUE(t.IsTranslation());

    Point p(Point3d{1, 0, 0});
    const Point3d src = p.Xyz();
    ApplyTransform(p, t);
    EXPECT_NEAR(p.Xyz().x(), 3.0, 1e-12);
    EXPECT_NEAR(p.Xyz().y(), 3.0, 1e-12);
    EXPECT_NEAR(p.Xyz().z(), 4.0, 1e-12);
    EXPECT_NEAR(src.x(), 1.0, 1e-12);

    LineCurve line(Point3d{0, 0, 0}, Vector3d{1, 0, 0});
    ASSERT_TRUE(ApplyTransform(line, t));
    EXPECT_NEAR(line.Origin().x(), 2.0, 1e-12);
    EXPECT_NEAR(line.Direction().x(), 1.0, 1e-12);

    PlaneSurface plane(Point3d{0, 0, 0}, Vector3d{0, 1, 0});
    const Point3d planeOrigin = plane.Origin();
    ASSERT_TRUE(ApplyTransform(plane, t));
    EXPECT_NEAR(plane.Origin().x(), 2.0, 1e-12);
    EXPECT_NEAR(plane.Origin().y(), 3.0, 1e-12);
    EXPECT_NEAR(planeOrigin.x(), 0.0, 1e-12);

    SphereSurface sphere(Point3d{0, 0, 0}, 1.5);
    ASSERT_TRUE(ApplyTransform(sphere, t));
    EXPECT_NEAR(sphere.Center().x(), 2.0, 1e-12);
    EXPECT_NEAR(sphere.Radius(), 1.5, 1e-12);
}

TEST(RigidTransform, IsTranslationFalseWhenRotated)
{
    RigidTransform t;
    t.XAxis = Vector3d{0, 1, 0};
    t.YAxis = Vector3d{-1, 0, 0};
    EXPECT_FALSE(t.IsTranslation());
}

TEST(ApplyTransform, TranslatesPrimitiveSpecs)
{
    const RigidTransform t{.Translation = Point3d{2, 3, 0}};

    auto boxOut = ApplyTransform(
        PrimitiveSpec{BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}}}, t);
    ASSERT_TRUE(boxOut.has_value());
    const auto* box = std::get_if<BoxSpec>(&*boxOut);
    ASSERT_NE(box, nullptr);
    EXPECT_NEAR(box->Min.x(), 2.0, 1e-12);
    EXPECT_NEAR(box->Min.y(), 3.0, 1e-12);
    EXPECT_NEAR(box->Max.x(), 3.0, 1e-12);
    EXPECT_NEAR(box->Max.z(), 1.0, 1e-12);

    auto sphereOut = ApplyTransform(
        PrimitiveSpec{SphereSpec{.Center = {0, 0, 0}, .Radius = 1.5}}, t);
    ASSERT_TRUE(sphereOut.has_value());
    const auto* sphere = std::get_if<SphereSpec>(&*sphereOut);
    ASSERT_NE(sphere, nullptr);
    EXPECT_NEAR(sphere->Center.x(), 2.0, 1e-12);
    EXPECT_NEAR(sphere->Center.y(), 3.0, 1e-12);
    EXPECT_NEAR(sphere->Radius, 1.5, 1e-12);

    auto bezierOut = ApplyTransform(
        PrimitiveSpec{BezierSpec{.Cvs = {{0, 0, 0},
                                         {0, 1, 0},
                                         {1, 1, 0},
                                         {1, 0, 0}}}},
        t);
    ASSERT_TRUE(bezierOut.has_value());
    const auto* bezier = std::get_if<BezierSpec>(&*bezierOut);
    ASSERT_NE(bezier, nullptr);
    EXPECT_NEAR(bezier->P0().x(), 2.0, 1e-12);
    EXPECT_NEAR(bezier->P3().y(), 3.0, 1e-12);
}

TEST(ApplyTransform, PrimitiveSpecRejectsRotation)
{
    RigidTransform t;
    t.XAxis = Vector3d{0, 1, 0};
    t.YAxis = Vector3d{-1, 0, 0};
    EXPECT_FALSE(
        ApplyTransform(PrimitiveSpec{BoxSpec{}}, t).has_value());
    EXPECT_FALSE(
        ApplyTransform(PrimitiveSpec{SphereSpec{}}, t).has_value());
    EXPECT_FALSE(
        ApplyTransform(PrimitiveSpec{BezierSpec{}}, t).has_value());
}

}  // namespace
}  // namespace brep
