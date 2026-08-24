#pragma once

#include <Eigen/Dense>

#include <cmath>
#include <ostream>

namespace brep
{

/// Free 3D direction / displacement (Eigen-backed).
class Vector3d
{
 public:
  Vector3d() = default;
  Vector3d(double x, double y, double z) : m_data(x, y, z)
  {
  }
  explicit Vector3d(const Eigen::Vector3d& v) : m_data(v)
  {
  }

  [[nodiscard]] double x() const noexcept
  {
      return m_data.x(); 
  }
  [[nodiscard]] double y() const noexcept
  {
      return m_data.y(); 
  }
  [[nodiscard]] double z() const noexcept
  {
      return m_data.z(); 
  }
  double& x() noexcept
  {
      return m_data.x(); 
  }
  double& y() noexcept
  {
      return m_data.y(); 
  }
  double& z() noexcept
  {
      return m_data.z(); 
  }

  [[nodiscard]] const Eigen::Vector3d& eigen() const noexcept
  {
      return m_data; 
  }
  [[nodiscard]] Eigen::Vector3d& eigen() noexcept
  {
      return m_data; 
  }

  [[nodiscard]] Vector3d operator+(const Vector3d& o) const
  {
    return Vector3d{m_data + o.m_data};
  }
  [[nodiscard]] Vector3d operator-(const Vector3d& o) const
  {
    return Vector3d{m_data - o.m_data};
  }
  [[nodiscard]] Vector3d operator*(double s) const
  {
      return Vector3d{m_data * s}; 
  }
  [[nodiscard]] Vector3d operator/(double s) const
  {
      return Vector3d{m_data / s}; 
  }
  [[nodiscard]] Vector3d operator-() const
  {
      return Vector3d{-m_data}; 
  }

  Vector3d& operator+=(const Vector3d& o)
  {
    m_data += o.m_data;
    return *this;
  }
  Vector3d& operator-=(const Vector3d& o)
  {
    m_data -= o.m_data;
    return *this;
  }
  Vector3d& operator*=(double s)
  {
    m_data *= s;
    return *this;
  }

  [[nodiscard]] double dot(const Vector3d& o) const
  {
      return m_data.dot(o.m_data); 
  }
  [[nodiscard]] Vector3d cross(const Vector3d& o) const
  {
    return Vector3d{m_data.cross(o.m_data)};
  }
  [[nodiscard]] double norm() const
  {
      return m_data.norm(); 
  }
  [[nodiscard]] double squaredNorm() const
  {
      return m_data.squaredNorm(); 
  }
  [[nodiscard]] Vector3d normalized() const
  {
    const double n = norm();
    return n > 0.0 ? Vector3d{m_data / n} : Vector3d{};
  }

 private:
  Eigen::Vector3d m_data{0.0, 0.0, 0.0};
};

inline Vector3d operator*(double s, const Vector3d& v)
{
    return v * s; 
}

inline std::ostream& operator<<(std::ostream& os, const Vector3d& v)
{
  return os << '(' << v.x() << ", " << v.y() << ", " << v.z() << ')';
}

/// Affine 3D position (Eigen-backed). Distinct from Vector3d.
class Point3d
{
 public:
  Point3d() = default;
  Point3d(double x, double y, double z) : m_data(x, y, z)
  {
  }
  explicit Point3d(const Eigen::Vector3d& v) : m_data(v)
  {
  }

  [[nodiscard]] double x() const noexcept
  {
      return m_data.x(); 
  }
  [[nodiscard]] double y() const noexcept
  {
      return m_data.y(); 
  }
  [[nodiscard]] double z() const noexcept
  {
      return m_data.z(); 
  }
  double& x() noexcept
  {
      return m_data.x(); 
  }
  double& y() noexcept
  {
      return m_data.y(); 
  }
  double& z() noexcept
  {
      return m_data.z(); 
  }

  [[nodiscard]] const Eigen::Vector3d& eigen() const noexcept
  {
      return m_data; 
  }
  [[nodiscard]] Eigen::Vector3d& eigen() noexcept
  {
      return m_data; 
  }

  [[nodiscard]] Point3d operator+(const Vector3d& v) const
  {
    return Point3d{m_data + v.eigen()};
  }
  [[nodiscard]] Point3d operator-(const Vector3d& v) const
  {
    return Point3d{m_data - v.eigen()};
  }
  [[nodiscard]] Vector3d operator-(const Point3d& o) const
  {
    return Vector3d{m_data - o.m_data};
  }

  Point3d& operator+=(const Vector3d& v)
  {
    m_data += v.eigen();
    return *this;
  }
  Point3d& operator-=(const Vector3d& v)
  {
    m_data -= v.eigen();
    return *this;
  }

  [[nodiscard]] double distance_to(const Point3d& o) const
  {
    return (*this - o).norm();
  }

 private:
  Eigen::Vector3d m_data{0.0, 0.0, 0.0};
};

inline std::ostream& operator<<(std::ostream& os, const Point3d& p)
{
  return os << '(' << p.x() << ", " << p.y() << ", " << p.z() << ')';
}

/// UV / parameter-space 2D point.
class Point2d
{
 public:
  Point2d() = default;
  Point2d(double u, double v) : m_data(u, v)
  {
  }
  explicit Point2d(const Eigen::Vector2d& v) : m_data(v)
  {
  }

  [[nodiscard]] double u() const noexcept
  {
      return m_data.x(); 
  }
  [[nodiscard]] double v() const noexcept
  {
      return m_data.y(); 
  }
  double& u() noexcept
  {
      return m_data.x(); 
  }
  double& v() noexcept
  {
      return m_data.y(); 
  }

  [[nodiscard]] const Eigen::Vector2d& eigen() const noexcept
  {
      return m_data; 
  }

 private:
  Eigen::Vector2d m_data{0.0, 0.0};
};

/// Backward-compatible alias used by older call sites / pcurve UV.
using Vec2 = Point2d;

}  // namespace brep
