#pragma once

#include "brep/math.hpp"
#include "brep/types.hpp"

#include <cmath>
#include <utility>

namespace brep {

// ---------------------------------------------------------------------------
// Geometry carriers (owned by Model; referenced by topology)
// ---------------------------------------------------------------------------

class Point : public Named {
 public:
  explicit Point(Vec3 xyz) : xyz_(xyz) {}

  [[nodiscard]] const Vec3& xyz() const noexcept { return xyz_; }
  void set_xyz(Vec3 p) noexcept { xyz_ = p; }

 private:
  Vec3 xyz_;
};

class Curve {
 public:
  virtual ~Curve() = default;

  [[nodiscard]] virtual CurveKind kind() const noexcept = 0;
  [[nodiscard]] virtual Vec3 eval(double t) const = 0;
  [[nodiscard]] virtual Vec3 tangent(double t) const = 0;
  [[nodiscard]] virtual std::pair<double, double> domain() const noexcept = 0;

  Id id{0};
};

class LineCurve final : public Curve {
 public:
  LineCurve(Vec3 origin, Vec3 direction)
      : origin_(origin), direction_(direction.normalized()) {}

  [[nodiscard]] CurveKind kind() const noexcept override { return CurveKind::Line; }
  [[nodiscard]] Vec3 eval(double t) const override { return origin_ + direction_ * t; }
  [[nodiscard]] Vec3 tangent(double /*t*/) const override { return direction_; }
  [[nodiscard]] std::pair<double, double> domain() const noexcept override {
    return {0.0, length_};
  }

  void set_length(double len) noexcept { length_ = len; }
  [[nodiscard]] double length() const noexcept { return length_; }
  [[nodiscard]] const Vec3& origin() const noexcept { return origin_; }
  [[nodiscard]] const Vec3& direction() const noexcept { return direction_; }

 private:
  Vec3 origin_;
  Vec3 direction_;
  double length_{1.0};
};

class CircleCurve final : public Curve {
 public:
  CircleCurve(Vec3 center, Vec3 normal, double radius)
      : center_(center), normal_(normal.normalized()), radius_(radius) {
    // Build an orthonormal frame in the plane.
    const Vec3 ref = std::abs(normal_.x) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    x_axis_ = normal_.cross(ref).normalized();
    y_axis_ = normal_.cross(x_axis_).normalized();
  }

  [[nodiscard]] CurveKind kind() const noexcept override { return CurveKind::Circle; }
  [[nodiscard]] Vec3 eval(double t) const override;
  [[nodiscard]] Vec3 tangent(double t) const override;
  [[nodiscard]] std::pair<double, double> domain() const noexcept override {
    return {0.0, 2.0 * 3.14159265358979323846};
  }

  [[nodiscard]] const Vec3& center() const noexcept { return center_; }
  [[nodiscard]] double radius() const noexcept { return radius_; }

 private:
  Vec3 center_;
  Vec3 normal_;
  Vec3 x_axis_;
  Vec3 y_axis_;
  double radius_;
};

/// 2D parameter-space curve sitting on a face (pcurve).
class Curve2d {
 public:
  virtual ~Curve2d() = default;
  [[nodiscard]] virtual Vec2 eval(double t) const = 0;
  [[nodiscard]] virtual std::pair<double, double> domain() const noexcept = 0;
  Id id{0};
};

class LineCurve2d final : public Curve2d {
 public:
  LineCurve2d(Vec2 a, Vec2 b) : a_(a), b_(b) {}

  [[nodiscard]] Vec2 eval(double t) const override {
    return {a_.u + (b_.u - a_.u) * t, a_.v + (b_.v - a_.v) * t};
  }
  [[nodiscard]] std::pair<double, double> domain() const noexcept override {
    return {0.0, 1.0};
  }

 private:
  Vec2 a_;
  Vec2 b_;
};

class Surface {
 public:
  virtual ~Surface() = default;

  [[nodiscard]] virtual SurfaceKind kind() const noexcept = 0;
  [[nodiscard]] virtual Vec3 eval(double u, double v) const = 0;
  [[nodiscard]] virtual Vec3 normal(double u, double v) const = 0;

  Id id{0};
};

class PlaneSurface final : public Surface {
 public:
  PlaneSurface(Vec3 origin, Vec3 normal)
      : origin_(origin), normal_(normal.normalized()) {
    const Vec3 ref = std::abs(normal_.x) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
    u_axis_ = normal_.cross(ref).normalized();
    v_axis_ = normal_.cross(u_axis_).normalized();
  }

  PlaneSurface(Vec3 origin, Vec3 u_axis, Vec3 v_axis)
      : origin_(origin),
        u_axis_(u_axis.normalized()),
        v_axis_(v_axis.normalized()),
        normal_(u_axis_.cross(v_axis_).normalized()) {}

  [[nodiscard]] SurfaceKind kind() const noexcept override { return SurfaceKind::Plane; }
  [[nodiscard]] Vec3 eval(double u, double v) const override {
    return origin_ + u_axis_ * u + v_axis_ * v;
  }
  [[nodiscard]] Vec3 normal(double /*u*/, double /*v*/) const override { return normal_; }

  [[nodiscard]] const Vec3& origin() const noexcept { return origin_; }
  [[nodiscard]] const Vec3& u_axis() const noexcept { return u_axis_; }
  [[nodiscard]] const Vec3& v_axis() const noexcept { return v_axis_; }

 private:
  Vec3 origin_;
  Vec3 u_axis_;
  Vec3 v_axis_;
  Vec3 normal_;
};

}  // namespace brep
