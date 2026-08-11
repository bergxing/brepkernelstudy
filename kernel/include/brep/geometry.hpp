#pragma once

#include "brep/math.hpp"
#include "brep/types.hpp"

#include <cmath>
#include <numbers>
#include <utility>

namespace brep {

// ---------------------------------------------------------------------------
// Geometry carriers (owned by Model; referenced by topology)
// ---------------------------------------------------------------------------

class Point : public Named {
 public:
  explicit Point(Point3d xyz) : xyz_(xyz) {}

  [[nodiscard]] const Point3d& xyz() const noexcept { return xyz_; }
  void set_xyz(Point3d p) noexcept { xyz_ = p; }

 private:
  Point3d xyz_;
};

class Curve {
 public:
  virtual ~Curve() = default;

  [[nodiscard]] virtual CurveKind kind() const noexcept = 0;
  [[nodiscard]] virtual Point3d eval(double t) const = 0;
  [[nodiscard]] virtual Vector3d tangent(double t) const = 0;
  [[nodiscard]] virtual std::pair<double, double> domain() const noexcept = 0;

  Id id{0};
};

class LineCurve final : public Curve {
 public:
  LineCurve(Point3d origin, Vector3d direction)
      : origin_(origin), direction_(direction.normalized()) {}

  [[nodiscard]] CurveKind kind() const noexcept override { return CurveKind::Line; }
  [[nodiscard]] Point3d eval(double t) const override {
    return origin_ + direction_ * t;
  }
  [[nodiscard]] Vector3d tangent(double /*t*/) const override { return direction_; }
  [[nodiscard]] std::pair<double, double> domain() const noexcept override {
    return {0.0, length_};
  }

  void set_length(double len) noexcept { length_ = len; }
  [[nodiscard]] double length() const noexcept { return length_; }
  [[nodiscard]] const Point3d& origin() const noexcept { return origin_; }
  [[nodiscard]] const Vector3d& direction() const noexcept { return direction_; }

 private:
  Point3d origin_;
  Vector3d direction_;
  double length_{1.0};
};

class CircleCurve final : public Curve {
 public:
  CircleCurve(Point3d center, Vector3d normal, double radius)
      : center_(center), normal_(normal.normalized()), radius_(radius) {
    const Vector3d ref =
        std::abs(normal_.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
    x_axis_ = normal_.cross(ref).normalized();
    y_axis_ = normal_.cross(x_axis_).normalized();
  }

  [[nodiscard]] CurveKind kind() const noexcept override { return CurveKind::Circle; }
  [[nodiscard]] Point3d eval(double t) const override;
  [[nodiscard]] Vector3d tangent(double t) const override;
  [[nodiscard]] std::pair<double, double> domain() const noexcept override {
    return {0.0, 2.0 * std::numbers::pi};
  }

  [[nodiscard]] const Point3d& center() const noexcept { return center_; }
  [[nodiscard]] double radius() const noexcept { return radius_; }

 private:
  Point3d center_;
  Vector3d normal_;
  Vector3d x_axis_;
  Vector3d y_axis_;
  double radius_;
};

/// 2D parameter-space curve sitting on a face (pcurve).
class Curve2d {
 public:
  virtual ~Curve2d() = default;
  [[nodiscard]] virtual Point2d eval(double t) const = 0;
  [[nodiscard]] virtual std::pair<double, double> domain() const noexcept = 0;
  Id id{0};
};

class LineCurve2d final : public Curve2d {
 public:
  LineCurve2d(Point2d a, Point2d b) : a_(a), b_(b) {}

  [[nodiscard]] Point2d eval(double t) const override {
    return Point2d{a_.u() + (b_.u() - a_.u()) * t, a_.v() + (b_.v() - a_.v()) * t};
  }
  [[nodiscard]] std::pair<double, double> domain() const noexcept override {
    return {0.0, 1.0};
  }

 private:
  Point2d a_;
  Point2d b_;
};

class Surface {
 public:
  virtual ~Surface() = default;

  [[nodiscard]] virtual SurfaceKind kind() const noexcept = 0;
  [[nodiscard]] virtual Point3d eval(double u, double v) const = 0;
  [[nodiscard]] virtual Vector3d normal(double u, double v) const = 0;

  Id id{0};
};

class PlaneSurface final : public Surface {
 public:
  PlaneSurface(Point3d origin, Vector3d normal)
      : origin_(origin), normal_(normal.normalized()) {
    const Vector3d ref =
        std::abs(normal_.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
    u_axis_ = normal_.cross(ref).normalized();
    v_axis_ = normal_.cross(u_axis_).normalized();
  }

  PlaneSurface(Point3d origin, Vector3d u_axis, Vector3d v_axis)
      : origin_(origin),
        u_axis_(u_axis.normalized()),
        v_axis_(v_axis.normalized()),
        normal_(u_axis_.cross(v_axis_).normalized()) {}

  [[nodiscard]] SurfaceKind kind() const noexcept override { return SurfaceKind::Plane; }
  [[nodiscard]] Point3d eval(double u, double v) const override {
    return origin_ + u_axis_ * u + v_axis_ * v;
  }
  [[nodiscard]] Vector3d normal(double /*u*/, double /*v*/) const override {
    return normal_;
  }

  [[nodiscard]] const Point3d& origin() const noexcept { return origin_; }
  [[nodiscard]] const Vector3d& u_axis() const noexcept { return u_axis_; }
  [[nodiscard]] const Vector3d& v_axis() const noexcept { return v_axis_; }

  /// Project a 3D point into the plane's UV parameter space.
  [[nodiscard]] Point2d param_of(const Point3d& p) const {
    const Vector3d d = p - origin_;
    return Point2d{d.dot(u_axis_), d.dot(v_axis_)};
  }

 private:
  Point3d origin_;
  Vector3d u_axis_;
  Vector3d v_axis_;
  Vector3d normal_;
};

/// Analytic sphere. UV: u = longitude [0, 2π), v = latitude [-π/2, +π/2]
/// (Y-up: north pole at center + (0, +R, 0)).
class SphereSurface final : public Surface {
 public:
  SphereSurface(Point3d center, double radius);

  [[nodiscard]] SurfaceKind kind() const noexcept override {
    return SurfaceKind::Sphere;
  }
  [[nodiscard]] Point3d eval(double u, double v) const override;
  [[nodiscard]] Vector3d normal(double u, double v) const override;

  /// Project a 3D point to sphere UV via direction from center (not clamped to
  /// surface radius). u ∈ [0, 2π); at exact poles u is defined as 0.
  [[nodiscard]] Point2d param_of(const Point3d& p) const;

  [[nodiscard]] const Point3d& center() const noexcept { return center_; }
  [[nodiscard]] double radius() const noexcept { return radius_; }

 private:
  Point3d center_;
  double radius_{1.0};
};

/// Infinite analytic cylinder.
/// UV: u = angle about axis [0, 2π); v = signed height along axis.
/// Frame: origin on axis; axis unit; x_axis/y_axis orthonormal, right-handed.
class CylinderSurface final : public Surface {
 public:
  CylinderSurface(Point3d origin, Vector3d axis, double radius);

  [[nodiscard]] SurfaceKind kind() const noexcept override {
    return SurfaceKind::Cylinder;
  }
  [[nodiscard]] Point3d eval(double u, double v) const override;
  [[nodiscard]] Vector3d normal(double u, double v) const override;

  /// Project to cylinder UV (radial direction ignored for radius; uses
  /// direction from axis).
  [[nodiscard]] Point2d param_of(const Point3d& p) const;

  [[nodiscard]] const Point3d& origin() const noexcept { return origin_; }
  [[nodiscard]] const Vector3d& axis() const noexcept { return axis_; }
  [[nodiscard]] const Vector3d& x_axis() const noexcept { return x_axis_; }
  [[nodiscard]] const Vector3d& y_axis() const noexcept { return y_axis_; }
  [[nodiscard]] double radius() const noexcept { return radius_; }

 private:
  Point3d origin_;
  Vector3d axis_;
  Vector3d x_axis_;
  Vector3d y_axis_;
  double radius_{1.0};
};

}  // namespace brep
