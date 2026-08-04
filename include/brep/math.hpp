#pragma once

#include <Eigen/Dense>

#include <cmath>
#include <ostream>

namespace brep {

/// Free 3D direction / displacement (Eigen-backed).
class Vector3d {
 public:
  Vector3d() = default;
  Vector3d(double x, double y, double z) : data_(x, y, z) {}
  explicit Vector3d(const Eigen::Vector3d& v) : data_(v) {}

  [[nodiscard]] double x() const noexcept { return data_.x(); }
  [[nodiscard]] double y() const noexcept { return data_.y(); }
  [[nodiscard]] double z() const noexcept { return data_.z(); }
  double& x() noexcept { return data_.x(); }
  double& y() noexcept { return data_.y(); }
  double& z() noexcept { return data_.z(); }

  [[nodiscard]] const Eigen::Vector3d& eigen() const noexcept { return data_; }
  [[nodiscard]] Eigen::Vector3d& eigen() noexcept { return data_; }

  [[nodiscard]] Vector3d operator+(const Vector3d& o) const {
    return Vector3d{data_ + o.data_};
  }
  [[nodiscard]] Vector3d operator-(const Vector3d& o) const {
    return Vector3d{data_ - o.data_};
  }
  [[nodiscard]] Vector3d operator*(double s) const { return Vector3d{data_ * s}; }
  [[nodiscard]] Vector3d operator/(double s) const { return Vector3d{data_ / s}; }
  [[nodiscard]] Vector3d operator-() const { return Vector3d{-data_}; }

  Vector3d& operator+=(const Vector3d& o) {
    data_ += o.data_;
    return *this;
  }
  Vector3d& operator-=(const Vector3d& o) {
    data_ -= o.data_;
    return *this;
  }
  Vector3d& operator*=(double s) {
    data_ *= s;
    return *this;
  }

  [[nodiscard]] double dot(const Vector3d& o) const { return data_.dot(o.data_); }
  [[nodiscard]] Vector3d cross(const Vector3d& o) const {
    return Vector3d{data_.cross(o.data_)};
  }
  [[nodiscard]] double norm() const { return data_.norm(); }
  [[nodiscard]] double squaredNorm() const { return data_.squaredNorm(); }
  [[nodiscard]] Vector3d normalized() const {
    const double n = norm();
    return n > 0.0 ? Vector3d{data_ / n} : Vector3d{};
  }

 private:
  Eigen::Vector3d data_{0.0, 0.0, 0.0};
};

inline Vector3d operator*(double s, const Vector3d& v) { return v * s; }

inline std::ostream& operator<<(std::ostream& os, const Vector3d& v) {
  return os << '(' << v.x() << ", " << v.y() << ", " << v.z() << ')';
}

/// Affine 3D position (Eigen-backed). Distinct from Vector3d.
class Point3d {
 public:
  Point3d() = default;
  Point3d(double x, double y, double z) : data_(x, y, z) {}
  explicit Point3d(const Eigen::Vector3d& v) : data_(v) {}

  [[nodiscard]] double x() const noexcept { return data_.x(); }
  [[nodiscard]] double y() const noexcept { return data_.y(); }
  [[nodiscard]] double z() const noexcept { return data_.z(); }
  double& x() noexcept { return data_.x(); }
  double& y() noexcept { return data_.y(); }
  double& z() noexcept { return data_.z(); }

  [[nodiscard]] const Eigen::Vector3d& eigen() const noexcept { return data_; }
  [[nodiscard]] Eigen::Vector3d& eigen() noexcept { return data_; }

  [[nodiscard]] Point3d operator+(const Vector3d& v) const {
    return Point3d{data_ + v.eigen()};
  }
  [[nodiscard]] Point3d operator-(const Vector3d& v) const {
    return Point3d{data_ - v.eigen()};
  }
  [[nodiscard]] Vector3d operator-(const Point3d& o) const {
    return Vector3d{data_ - o.data_};
  }

  Point3d& operator+=(const Vector3d& v) {
    data_ += v.eigen();
    return *this;
  }
  Point3d& operator-=(const Vector3d& v) {
    data_ -= v.eigen();
    return *this;
  }

  [[nodiscard]] double distance_to(const Point3d& o) const {
    return (*this - o).norm();
  }

 private:
  Eigen::Vector3d data_{0.0, 0.0, 0.0};
};

inline std::ostream& operator<<(std::ostream& os, const Point3d& p) {
  return os << '(' << p.x() << ", " << p.y() << ", " << p.z() << ')';
}

/// UV / parameter-space 2D point.
class Point2d {
 public:
  Point2d() = default;
  Point2d(double u, double v) : data_(u, v) {}
  explicit Point2d(const Eigen::Vector2d& v) : data_(v) {}

  [[nodiscard]] double u() const noexcept { return data_.x(); }
  [[nodiscard]] double v() const noexcept { return data_.y(); }
  double& u() noexcept { return data_.x(); }
  double& v() noexcept { return data_.y(); }

  [[nodiscard]] const Eigen::Vector2d& eigen() const noexcept { return data_; }

 private:
  Eigen::Vector2d data_{0.0, 0.0};
};

/// Backward-compatible alias used by older call sites / pcurve UV.
using Vec2 = Point2d;

}  // namespace brep
