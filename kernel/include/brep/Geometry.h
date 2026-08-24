#pragma once

#include "brep/Math.h"
#include "brep/Types.h"

#include <cmath>
#include <numbers>
#include <utility>

namespace brep
{

// ---------------------------------------------------------------------------
// Geometry carriers (owned by Model; referenced by topology)
// ---------------------------------------------------------------------------

class Point : public Named
{
 public:
  explicit Point(Point3d xyz) : m_xyz(xyz)
 {
  }

  [[nodiscard]] const Point3d& Xyz() const noexcept
  {
    return m_xyz;
  }
  void SetXyz(Point3d p) noexcept
  {
    m_xyz = p;
  }

 private:
  Point3d m_xyz;
};

class Curve
{
 public:
  virtual ~Curve() = default;

  [[nodiscard]] virtual CurveKind Kind() const noexcept = 0;
  [[nodiscard]] virtual Point3d Eval(double t) const = 0;
  [[nodiscard]] virtual Vector3d Tangent(double t) const = 0;
  [[nodiscard]] virtual std::pair<double, double> Domain() const noexcept = 0;

  Id id{0};
};

class LineCurve final : public Curve
{
 public:
  LineCurve(Point3d origin, Vector3d direction)
      : m_origin(origin), m_direction(direction.normalized())
  {
      }

  [[nodiscard]] CurveKind Kind() const noexcept override
  {
      return CurveKind::Line; 
  }
  [[nodiscard]] Point3d Eval(double t) const override
  {
    return m_origin + m_direction * t;
  }
  [[nodiscard]] Vector3d Tangent(double /*t*/) const override
  {
    return m_direction;
  }
  [[nodiscard]] std::pair<double, double> Domain() const noexcept override
  {
    return {0.0, m_length};
  }

  void SetLength(double len) noexcept
  {
    m_length = len;
  }
  [[nodiscard]] double Length() const noexcept
  {
    return m_length;
  }
  [[nodiscard]] const Point3d& Origin() const noexcept
  {
    return m_origin;
  }
  [[nodiscard]] const Vector3d& Direction() const noexcept
  {
    return m_direction;
  }

 private:
  Point3d m_origin;
  Vector3d m_direction;
  double m_length{1.0};
};

class CircleCurve final : public Curve
{
 public:
  CircleCurve(Point3d center, Vector3d normal, double radius)
      : m_center(center), m_normal(normal.normalized()), m_radius(radius)
  {
    const Vector3d ref =
        std::abs(m_normal.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
    m_xAxis = m_normal.cross(ref).normalized();
    m_yAxis = m_normal.cross(m_xAxis).normalized();
  }

  [[nodiscard]] CurveKind Kind() const noexcept override
  {
      return CurveKind::Circle; 
  }
  [[nodiscard]] Point3d Eval(double t) const override;
  [[nodiscard]] Vector3d Tangent(double t) const override;
  [[nodiscard]] std::pair<double, double> Domain() const noexcept override
  {
    return {0.0, 2.0 * std::numbers::pi};
  }

  [[nodiscard]] const Point3d& Center() const noexcept
  {
    return m_center;
  }
  [[nodiscard]] double Radius() const noexcept
  {
    return m_radius;
  }

 private:
  Point3d m_center;
  Vector3d m_normal;
  Vector3d m_xAxis;
  Vector3d m_yAxis;
  double m_radius;
};

/// 2D parameter-space curve sitting on a face (pcurve).
class Curve2d
{
 public:
  virtual ~Curve2d() = default;
  [[nodiscard]] virtual Point2d Eval(double t) const = 0;
  [[nodiscard]] virtual std::pair<double, double> Domain() const noexcept = 0;
  Id id{0};
};

class LineCurve2d final : public Curve2d
{
 public:
  LineCurve2d(Point2d a, Point2d b) : m_a(a), m_b(b)
 {
  }

  [[nodiscard]] Point2d Eval(double t) const override
 {
    return Point2d{m_a.u() + (m_b.u() - m_a.u()) * t, m_a.v() + (m_b.v() - m_a.v()) * t};
  }
  [[nodiscard]] std::pair<double, double> Domain() const noexcept override
 {
    return {0.0, 1.0};
  }

 private:
  Point2d m_a;
  Point2d m_b;
};

class Surface
{
 public:
  virtual ~Surface() = default;

  [[nodiscard]] virtual SurfaceKind Kind() const noexcept = 0;
  [[nodiscard]] virtual Point3d Eval(double u, double v) const = 0;
  [[nodiscard]] virtual Vector3d Normal(double u, double v) const = 0;

  Id id{0};
};

class PlaneSurface final : public Surface
{
 public:
  PlaneSurface(Point3d origin, Vector3d normal)
      : m_origin(origin), m_normal(normal.normalized())
  {
    const Vector3d ref =
        std::abs(m_normal.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
    m_uAxis = m_normal.cross(ref).normalized();
    m_vAxis = m_normal.cross(m_uAxis).normalized();
  }

  PlaneSurface(Point3d origin, Vector3d u_axis, Vector3d v_axis)
      : m_origin(origin),
        m_uAxis(u_axis.normalized()),
        m_vAxis(v_axis.normalized()),
        m_normal(m_uAxis.cross(m_vAxis).normalized())
        {
        }

  [[nodiscard]] SurfaceKind Kind() const noexcept override
        {
      return SurfaceKind::Plane; 
  }
  [[nodiscard]] Point3d Eval(double u, double v) const override
        {
    return m_origin + m_uAxis * u + m_vAxis * v;
  }
  [[nodiscard]] Vector3d Normal(double /*u*/, double /*v*/) const override
  {
    return m_normal;
  }

  [[nodiscard]] const Point3d& Origin() const noexcept
  {
    return m_origin;
  }
  [[nodiscard]] const Vector3d& UAxis() const noexcept
  {
    return m_uAxis;
  }
  [[nodiscard]] const Vector3d& VAxis() const noexcept
  {
    return m_vAxis;
  }

  /// Project a 3D point into the plane's UV parameter space.
  [[nodiscard]] Point2d ParamOf(const Point3d& p) const
  {
    const Vector3d d = p - m_origin;
    return Point2d{d.dot(m_uAxis), d.dot(m_vAxis)};
  }

 private:
  Point3d m_origin;
  Vector3d m_uAxis;
  Vector3d m_vAxis;
  Vector3d m_normal;
};

/// Analytic sphere. UV: u = longitude [0, 2π), v = latitude [-π/2, +π/2]
/// (Y-up: north pole at center + (0, +R, 0)).
class SphereSurface final : public Surface
{
 public:
  SphereSurface(Point3d center, double radius);

  [[nodiscard]] SurfaceKind Kind() const noexcept override
  {
    return SurfaceKind::Sphere;
  }
  [[nodiscard]] Point3d Eval(double u, double v) const override;
  [[nodiscard]] Vector3d Normal(double u, double v) const override;

  /// Project a 3D point to sphere UV via direction from center (not clamped to
  /// surface radius). u ∈ [0, 2π); at exact poles u is defined as 0.
  [[nodiscard]] Point2d ParamOf(const Point3d& p) const;

  [[nodiscard]] const Point3d& Center() const noexcept
  {
    return m_center;
  }
  [[nodiscard]] double Radius() const noexcept
  {
    return m_radius;
  }

 private:
  Point3d m_center;
  double m_radius{1.0};
};

/// Infinite analytic cylinder.
/// UV: u = angle about axis [0, 2π); v = signed height along axis.
/// Frame: origin on axis; axis unit; x_axis/y_axis orthonormal, right-handed.
class CylinderSurface final : public Surface
{
 public:
  CylinderSurface(Point3d origin, Vector3d axis, double radius);

  [[nodiscard]] SurfaceKind Kind() const noexcept override
  {
    return SurfaceKind::Cylinder;
  }
  [[nodiscard]] Point3d Eval(double u, double v) const override;
  [[nodiscard]] Vector3d Normal(double u, double v) const override;

  /// Project to cylinder UV (radial direction ignored for radius; uses
  /// direction from axis).
  [[nodiscard]] Point2d ParamOf(const Point3d& p) const;

  [[nodiscard]] const Point3d& Origin() const noexcept
  {
    return m_origin;
  }
  [[nodiscard]] const Vector3d& Axis() const noexcept
  {
    return m_axis;
  }
  [[nodiscard]] const Vector3d& XAxis() const noexcept
  {
    return m_xAxis;
  }
  [[nodiscard]] const Vector3d& YAxis() const noexcept
  {
    return m_yAxis;
  }
  [[nodiscard]] double Radius() const noexcept
  {
    return m_radius;
  }

 private:
  Point3d m_origin;
  Vector3d m_axis;
  Vector3d m_xAxis;
  Vector3d m_yAxis;
  double m_radius{1.0};
};

}  // namespace brep
