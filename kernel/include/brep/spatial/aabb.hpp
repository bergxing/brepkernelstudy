#pragma once

#include "brep/math.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace brep::spatial {

struct Aabb {
  Point3d min{std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity()};
  Point3d max{-std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()};

  [[nodiscard]] bool empty() const noexcept {
    return !(min.x() <= max.x() && min.y() <= max.y() && min.z() <= max.z());
  }

  [[nodiscard]] bool overlaps(const Aabb& o) const noexcept {
    return min.x() <= o.max.x() && max.x() >= o.min.x() &&
           min.y() <= o.max.y() && max.y() >= o.min.y() &&
           min.z() <= o.max.z() && max.z() >= o.min.z();
  }

  [[nodiscard]] double surface_area() const noexcept {
    if (empty()) return 0.0;
    const double dx = max.x() - min.x();
    const double dy = max.y() - min.y();
    const double dz = max.z() - min.z();
    return 2.0 * (dx * dy + dy * dz + dz * dx);
  }

  [[nodiscard]] Point3d center() const noexcept {
    return Point3d{0.5 * (min.x() + max.x()), 0.5 * (min.y() + max.y()),
                   0.5 * (min.z() + max.z())};
  }

  void expand(const Point3d& p) noexcept {
    min = Point3d{std::min(min.x(), p.x()), std::min(min.y(), p.y()),
                  std::min(min.z(), p.z())};
    max = Point3d{std::max(max.x(), p.x()), std::max(max.y(), p.y()),
                  std::max(max.z(), p.z())};
  }

  void expand(const Aabb& o) noexcept {
    if (o.empty()) return;
    expand(o.min);
    expand(o.max);
  }

  [[nodiscard]] static Aabb merge(const Aabb& a, const Aabb& b) {
    Aabb out;
    out.expand(a);
    out.expand(b);
    return out;
  }

  [[nodiscard]] int longest_axis() const noexcept {
    const double dx = max.x() - min.x();
    const double dy = max.y() - min.y();
    const double dz = max.z() - min.z();
    if (dx >= dy && dx >= dz) return 0;
    if (dy >= dz) return 1;
    return 2;
  }
};

}  // namespace brep::spatial
