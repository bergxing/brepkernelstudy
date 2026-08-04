#include "brep/geometry.hpp"

#include <cmath>

namespace brep {

Point3d CircleCurve::eval(double t) const {
  return center_ + x_axis_ * (radius_ * std::cos(t)) +
         y_axis_ * (radius_ * std::sin(t));
}

Vector3d CircleCurve::tangent(double t) const {
  return (x_axis_ * (-radius_ * std::sin(t)) + y_axis_ * (radius_ * std::cos(t)))
      .normalized();
}

}  // namespace brep
