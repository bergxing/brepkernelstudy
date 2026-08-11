#include "api/core.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace brep {
namespace {

constexpr double kEps = 1e-9;
using std::numbers::pi;

TEST(CylinderSurface, KindAndAccessors) {
  Model model;
  CylinderSurface* c =
      model.make_cylinder_surface(Point3d{0, 0, 0}, Vector3d{0, 1, 0}, 2.0);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->kind(), SurfaceKind::Cylinder);
  EXPECT_NEAR(c->radius(), 2.0, kEps);
  EXPECT_NEAR(c->axis().y(), 1.0, kEps);
}

TEST(CylinderSurface, EvalOnAxes) {
  CylinderSurface c(Point3d{0, 0, 0}, Vector3d{0, 1, 0}, 1.0);
  // u=0 → along x_axis (Y-axis cylinder: x_axis ≈ Z×Y? axis=Y, ref=X →
  // x = Y×X = -Z? Wait: Y×X = -Z. Actually axis.cross(ref)=Y×X=-Z
  // y_axis = Y×(-Z)= -X? Let's just check radius and height.
  const Point3d p = c.eval(0.0, 3.0);
  EXPECT_NEAR((p - Point3d{0, 3, 0}).norm(), 1.0, kEps);
  EXPECT_NEAR(p.y(), 3.0, kEps);

  const Point3d q = c.eval(pi / 2.0, 0.0);
  EXPECT_NEAR(q.y(), 0.0, kEps);
  EXPECT_NEAR((q - Point3d{0, 0, 0}).norm(), 1.0, kEps);
}

TEST(CylinderSurface, NormalRadialUnit) {
  CylinderSurface c(Point3d{1, 2, 3}, Vector3d{0, 0, 1}, 1.5);
  for (double u : {0.0, pi / 3.0, pi, 5.0 * pi / 3.0}) {
    const Point3d p = c.eval(u, 4.0);
    const Vector3d n = c.normal(u, 4.0);
    EXPECT_NEAR(n.norm(), 1.0, 1e-12);
    EXPECT_NEAR(n.dot(c.axis()), 0.0, 1e-12);
    const Vector3d radial = (p - c.origin()) - c.axis() * (p - c.origin()).dot(c.axis());
    EXPECT_NEAR(n.x(), radial.normalized().x(), 1e-12);
    EXPECT_NEAR(n.y(), radial.normalized().y(), 1e-12);
    EXPECT_NEAR(n.z(), radial.normalized().z(), 1e-12);
  }
}

TEST(CylinderSurface, ParamOfRoundTrip) {
  CylinderSurface c(Point3d{2, -1, 0.5}, Vector3d{1, 1, 0}, 3.0);
  const double samples[][2] = {
      {0.0, 0.0},
      {pi / 2.0, 1.5},
      {pi, -2.0},
      {3.0 * pi / 2.0, 10.0},
  };
  for (const auto& uv : samples) {
    const Point3d p = c.eval(uv[0], uv[1]);
    const Point2d q = c.param_of(p);
    EXPECT_NEAR(q.u(), uv[0], 1e-9) << "u";
    EXPECT_NEAR(q.v(), uv[1], 1e-9) << "v";
  }
}

}  // namespace
}  // namespace brep
