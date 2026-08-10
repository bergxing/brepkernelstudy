#include "brep/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace brep {

Point3d CircleCurve::eval(double t) const {
  return center_ + x_axis_ * (radius_ * std::cos(t)) +
         y_axis_ * (radius_ * std::sin(t));
}

Vector3d CircleCurve::tangent(double t) const {
  return (x_axis_ * (-radius_ * std::sin(t)) + y_axis_ * (radius_ * std::cos(t)))
      .normalized();
}

SphereSurface::SphereSurface(Point3d center, double radius)
    : center_(center), radius_(radius) {
  if (!(radius_ > 0.0)) {
    throw std::invalid_argument("SphereSurface: radius must be positive");
  }
}

Point3d SphereSurface::eval(double u, double v) const {
  const double cv = std::cos(v);
  const double sv = std::sin(v);
  const double cu = std::cos(u);
  const double su = std::sin(u);
  return Point3d{center_.x() + radius_ * cv * cu,
                 center_.y() + radius_ * sv,
                 center_.z() + radius_ * cv * su};
}

Vector3d SphereSurface::normal(double u, double v) const {
  const double cv = std::cos(v);
  const double sv = std::sin(v);
  const double cu = std::cos(u);
  const double su = std::sin(u);
  return Vector3d{cv * cu, sv, cv * su}.normalized();
}

Point2d SphereSurface::param_of(const Point3d& p) const {
  const Vector3d d = p - center_;
  const double len = d.norm();
  if (len < 1e-15) {
    return Point2d{0.0, 0.0};
  }
  const double inv = 1.0 / len;
  const double y = std::clamp(d.y() * inv, -1.0, 1.0);
  const double v = std::asin(y);
  const double horiz = std::sqrt(std::max(0.0, d.x() * d.x() + d.z() * d.z()));
  double u = 0.0;
  if (horiz > 1e-15) {
    u = std::atan2(d.z(), d.x());  // (-π, π]
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    if (u < 0.0) u += kTwoPi;
    if (u >= kTwoPi) u = 0.0;
  }
  return Point2d{u, v};
}

}  // namespace brep
