#include "brep/geometry.hpp"

#include <cmath>

namespace brep {

Vec3 CircleCurve::eval(double t) const {
  return center_ + x_axis_ * (radius_ * std::cos(t)) +
         y_axis_ * (radius_ * std::sin(t));
}

Vec3 CircleCurve::tangent(double t) const {
  return (x_axis_ * (-radius_ * std::sin(t)) + y_axis_ * (radius_ * std::cos(t)))
      .normalized();
}

}  // namespace brep
