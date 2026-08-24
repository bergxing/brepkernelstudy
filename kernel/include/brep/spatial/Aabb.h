#pragma once

#include "brep/Math.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace brep::spatial
{

struct Aabb
{
  Point3d Min{std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity()};
  Point3d Max{-std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()};

  [[nodiscard]] bool Empty() const noexcept
  {
    return !(Min.x() <= Max.x() && Min.y() <= Max.y() && Min.z() <= Max.z());
  }

  [[nodiscard]] bool Overlaps(const Aabb& o) const noexcept
  {
    return Min.x() <= o.Max.x() && Max.x() >= o.Min.x() &&
           Min.y() <= o.Max.y() && Max.y() >= o.Min.y() &&
           Min.z() <= o.Max.z() && Max.z() >= o.Min.z();
  }

  [[nodiscard]] double SurfaceArea() const noexcept
  {
    if (Empty()) return 0.0;
    const double dx = Max.x() - Min.x();
    const double dy = Max.y() - Min.y();
    const double dz = Max.z() - Min.z();
    return 2.0 * (dx * dy + dy * dz + dz * dx);
  }

  [[nodiscard]] Point3d Center() const noexcept
  {
    return Point3d{0.5 * (Min.x() + Max.x()), 0.5 * (Min.y() + Max.y()),
                   0.5 * (Min.z() + Max.z())};
  }

  void Expand(const Point3d& p) noexcept
  {
    Min = Point3d{std::min(Min.x(), p.x()), std::min(Min.y(), p.y()),
                  std::min(Min.z(), p.z())};
    Max = Point3d{std::max(Max.x(), p.x()), std::max(Max.y(), p.y()),
                  std::max(Max.z(), p.z())};
  }

  void Expand(const Aabb& o) noexcept
  {
    if (o.Empty()) return;
    Expand(o.Min);
    Expand(o.Max);
  }

  [[nodiscard]] static Aabb Merge(const Aabb& a, const Aabb& b)
  {
    Aabb out;
    out.Expand(a);
    out.Expand(b);
    return out;
  }

  [[nodiscard]] int LongestAxis() const noexcept
  {
    const double dx = Max.x() - Min.x();
    const double dy = Max.y() - Min.y();
    const double dz = Max.z() - Min.z();
    if (dx >= dy && dx >= dz) return 0;
    if (dy >= dz) return 1;
    return 2;
  }
};

}  // namespace brep::spatial
