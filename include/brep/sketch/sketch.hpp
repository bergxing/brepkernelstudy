#pragma once

#include "brep/math.hpp"
#include "brep/param/parameter.hpp"
#include "brep/plane.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace brep::sketch {

struct SketchEntityId {
  std::uint32_t value{0};
  [[nodiscard]] friend bool operator==(SketchEntityId a,
                                       SketchEntityId b) noexcept {
    return a.value == b.value;
  }
};

enum class EntityKind { Point, Line, Circle, Arc };

struct SketchPoint {
  Point2d p{};
  bool fixed{false};
};

struct SketchLine {
  SketchEntityId p0{};
  SketchEntityId p1{};
};

struct SketchCircle {
  SketchEntityId center{};
  double radius{1.0};
};

enum class ConstraintKind {
  Fixed,
  Distance,
  Horizontal,
  Vertical,
  Parallel,
  Perpendicular,
  Coincident,
  Equal,
  Radius,
  Angle
};

struct ConstraintId {
  std::uint32_t value{0};
};

struct Constraint {
  ConstraintId id{};
  ConstraintKind kind{ConstraintKind::Fixed};
  SketchEntityId a{};
  SketchEntityId b{};
  param::ParameterId dim{};
  double aux{0.0};
};

class Sketch {
 public:
  SketchEntityId add_point(Point2d p);
  SketchEntityId add_line(SketchEntityId p0, SketchEntityId p1);
  SketchEntityId add_circle(SketchEntityId center, double radius);

  ConstraintId add_constraint(Constraint c);

  [[nodiscard]] Plane frame() const { return frame_; }
  void set_frame(Plane frame) { frame_ = std::move(frame); }

  [[nodiscard]] SketchPoint* point(SketchEntityId id);
  [[nodiscard]] const SketchPoint* point(SketchEntityId id) const;
  [[nodiscard]] const std::vector<SketchPoint>& points() const { return points_; }
  [[nodiscard]] const std::vector<SketchLine>& lines() const { return lines_; }
  [[nodiscard]] std::vector<SketchCircle>& circles() { return circles_; }
  [[nodiscard]] const std::vector<SketchCircle>& circles() const {
    return circles_;
  }
  [[nodiscard]] const std::vector<Constraint>& constraints() const {
    return constraints_;
  }
  std::vector<Constraint>& constraints() { return constraints_; }

  [[nodiscard]] bool is_point(SketchEntityId id) const;
  [[nodiscard]] std::optional<SketchLine> line_at(std::size_t index) const;

 private:
  Plane frame_{Plane::xz_y_up()};
  std::vector<SketchPoint> points_;
  std::vector<SketchLine> lines_;
  std::vector<SketchCircle> circles_;
  std::vector<Constraint> constraints_;
  std::uint32_t next_entity_{1};
  std::uint32_t next_constraint_{1};
};

}  // namespace brep::sketch
