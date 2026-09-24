#pragma once

#include "brep/Math.h"

namespace brep::internal
{

[[nodiscard]] inline bool PointsEqual(const Point3d& a, const Point3d& b,
                                      double eps)
{
  return (a - b).norm() <= eps;
}

[[nodiscard]] inline double SnapTolerance(double eps)
{
  return eps * (1.0 + 1e-6);
}

}  // namespace brep::internal
