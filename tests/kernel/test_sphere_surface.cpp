#include "api/core.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace brep {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

TEST(SphereSurface, KindAndAccessors) {
  Model model;
  SphereSurface* s = model.make_sphere_surface(Point3d{1, 2, 3}, 4.0);
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->kind(), SurfaceKind::Sphere);
  EXPECT_DOUBLE_EQ(s->center().x(), 1.0);
  EXPECT_DOUBLE_EQ(s->center().y(), 2.0);
  EXPECT_DOUBLE_EQ(s->center().z(), 3.0);
  EXPECT_DOUBLE_EQ(s->radius(), 4.0);
}

TEST(SphereSurface, EquatorAndPolesEval) {
  const Point3d c{0, 0, 0};
  const double r = 2.0;
  SphereSurface s(c, r);

  // North pole v = +π/2
  const Point3d north = s.eval(0.0, kPi / 2.0);
  EXPECT_NEAR(north.x(), 0.0, kEps);
  EXPECT_NEAR(north.y(), r, kEps);
  EXPECT_NEAR(north.z(), 0.0, kEps);

  // South pole v = -π/2 (u irrelevant)
  const Point3d south = s.eval(1.234, -kPi / 2.0);
  EXPECT_NEAR(south.x(), 0.0, kEps);
  EXPECT_NEAR(south.y(), -r, kEps);
  EXPECT_NEAR(south.z(), 0.0, kEps);

  // Equator v = 0, u = 0 → +X
  const Point3d eq_x = s.eval(0.0, 0.0);
  EXPECT_NEAR(eq_x.x(), r, kEps);
  EXPECT_NEAR(eq_x.y(), 0.0, kEps);
  EXPECT_NEAR(eq_x.z(), 0.0, kEps);

  // Equator u = π/2 → +Z
  const Point3d eq_z = s.eval(kPi / 2.0, 0.0);
  EXPECT_NEAR(eq_z.x(), 0.0, kEps);
  EXPECT_NEAR(eq_z.y(), 0.0, kEps);
  EXPECT_NEAR(eq_z.z(), r, kEps);
}

TEST(SphereSurface, NormalsUnitAndOutward) {
  SphereSurface s(Point3d{1, 1, 1}, 3.0);

  const auto check = [&](double u, double v) {
    const Point3d p = s.eval(u, v);
    const Vector3d n = s.normal(u, v);
    EXPECT_NEAR(n.norm(), 1.0, 1e-12);
    const Vector3d expected = (p - s.center()).normalized();
    EXPECT_NEAR(n.x(), expected.x(), 1e-12);
    EXPECT_NEAR(n.y(), expected.y(), 1e-12);
    EXPECT_NEAR(n.z(), expected.z(), 1e-12);
  };

  check(0.0, 0.0);
  check(kPi / 4.0, kPi / 6.0);
  check(0.0, kPi / 2.0);
  check(2.0, -kPi / 2.0);
}

TEST(SphereSurface, ParamOfRoundTrip) {
  SphereSurface s(Point3d{10, -2, 5}, 7.5);
  const double samples[][2] = {
      {0.0, 0.0},
      {kPi / 3.0, kPi / 5.0},
      {5.0 * kPi / 3.0, -kPi / 4.0},
      {0.1, kPi / 2.0 - 1e-6},  // near north (avoid exact pole u ambiguity)
      {2.0, -kPi / 2.0 + 1e-6},
  };

  for (const auto& uv : samples) {
    const Point3d p = s.eval(uv[0], uv[1]);
    const Point2d got = s.param_of(p);
    // Round-trip through eval again (u at poles is ill-defined; near-pole ok)
    const Point3d p2 = s.eval(got.u(), got.v());
    EXPECT_NEAR((p2 - p).norm(), 0.0, 1e-8) << "u=" << uv[0] << " v=" << uv[1];
    EXPECT_GE(got.u(), 0.0);
    EXPECT_LT(got.u(), 2.0 * kPi + 1e-12);
    EXPECT_GE(got.v(), -kPi / 2.0 - 1e-12);
    EXPECT_LE(got.v(), kPi / 2.0 + 1e-12);
  }
}

}  // namespace
}  // namespace brep
