#pragma once

#include "brep/Math.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace brep::internal
{

[[nodiscard]] inline double SignedArea2d(const std::vector<Point2d>& ring)
{
  double area = 0.0;
  const std::size_t n = ring.size();
  for (std::size_t i = 0; i < n; ++i)
  {
    const std::size_t j = (i + 1) % n;
    area += ring[i].u() * ring[j].v() - ring[j].u() * ring[i].v();
  }
  return 0.5 * area;
}

[[nodiscard]] inline bool PointOnSegment2d(const Point2d& point,
                                           const Point2d& a, const Point2d& b,
                                           double eps)
{
  const double dx = b.u() - a.u();
  const double dy = b.v() - a.v();
  const double cross =
      (point.u() - a.u()) * dy - (point.v() - a.v()) * dx;
  if (std::abs(cross) > eps * std::max(1.0, std::hypot(dx, dy)))
  {
    return false;
  }
  const double dot = (point.u() - a.u()) * (point.u() - b.u()) +
                     (point.v() - a.v()) * (point.v() - b.v());
  return dot <= eps * eps;
}

[[nodiscard]] inline bool PointInPolygon2d(const Point2d& point,
                                           const std::vector<Point2d>& polygon)
{
  if (polygon.size() < 3U)
  {
    return false;
  }
  bool inside = false;
  for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
  {
    const Point2d& a = polygon[i];
    const Point2d& b = polygon[j];
    if ((a.v() > point.v()) != (b.v() > point.v()) &&
        point.u() < (b.u() - a.u()) * (point.v() - a.v()) / (b.v() - a.v()) +
                        a.u())
    {
      inside = !inside;
    }
  }
  return inside;
}

[[nodiscard]] inline bool PointInPolygon2d(const Point2d& point,
                                           const std::vector<Point2d>& polygon,
                                           double eps)
{
  if (eps <= 0.0)
  {
    return PointInPolygon2d(point, polygon);
  }
  if (polygon.size() < 3U)
  {
    return false;
  }
  bool inside = false;
  for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
  {
    if (PointOnSegment2d(point, polygon[j], polygon[i], eps))
    {
      return true;
    }
    const double yi = polygon[i].v();
    const double yj = polygon[j].v();
    const double xi = polygon[i].u();
    const double xj = polygon[j].u();
    const bool intersect =
        ((yi > point.v()) != (yj > point.v())) &&
        (point.u() < (xj - xi) * (point.v() - yi) / (yj - yi + 0.0) + xi);
    if (intersect)
    {
      inside = !inside;
    }
  }
  return inside;
}

}  // namespace brep::internal
