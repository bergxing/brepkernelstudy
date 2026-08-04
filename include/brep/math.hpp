#pragma once

#include <cmath>
#include <ostream>

namespace brep {

struct Vec3 {
  double x{0};
  double y{0};
  double z{0};

  constexpr Vec3() = default;
  constexpr Vec3(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}

  [[nodiscard]] constexpr Vec3 operator+(const Vec3& o) const noexcept {
    return {x + o.x, y + o.y, z + o.z};
  }
  [[nodiscard]] constexpr Vec3 operator-(const Vec3& o) const noexcept {
    return {x - o.x, y - o.y, z - o.z};
  }
  [[nodiscard]] constexpr Vec3 operator*(double s) const noexcept {
    return {x * s, y * s, z * s};
  }
  [[nodiscard]] constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }

  Vec3& operator+=(const Vec3& o) noexcept {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }

  [[nodiscard]] constexpr double dot(const Vec3& o) const noexcept {
    return x * o.x + y * o.y + z * o.z;
  }
  [[nodiscard]] constexpr Vec3 cross(const Vec3& o) const noexcept {
    return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
  }
  [[nodiscard]] double norm() const noexcept { return std::sqrt(dot(*this)); }
  [[nodiscard]] Vec3 normalized() const {
    const double n = norm();
    return n > 0 ? (*this) * (1.0 / n) : Vec3{};
  }
  [[nodiscard]] double distance_to(const Vec3& o) const noexcept {
    return (*this - o).norm();
  }
};

inline constexpr Vec3 operator*(double s, const Vec3& v) noexcept { return v * s; }

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
  return os << '(' << v.x << ", " << v.y << ", " << v.z << ')';
}

struct Vec2 {
  double u{0};
  double v{0};

  constexpr Vec2() = default;
  constexpr Vec2(double u_, double v_) noexcept : u(u_), v(v_) {}
};

}  // namespace brep
