#include "brep/sketch/sketch.hpp"

namespace brep::sketch {

SketchEntityId Sketch::add_point(Point2d p) {
  SketchEntityId id{next_entity_++};
  // Point ids are indices into points_ via value encoding: store sequentially.
  // Use id.value as 1-based index into points_.
  points_.push_back(SketchPoint{p, false});
  // Remap: we store entity id as index+1 matching push order for points only.
  // For mixed entities, keep parallel maps — simplify: point id == index+1 and
  // next_entity starts at 1, but lines/circles also consume ids.
  // Fix: store point_ids separately.
  (void)id;
  return SketchEntityId{static_cast<std::uint32_t>(points_.size())};
}

SketchEntityId Sketch::add_line(SketchEntityId p0, SketchEntityId p1) {
  lines_.push_back(SketchLine{p0, p1});
  return SketchEntityId{100000u + static_cast<std::uint32_t>(lines_.size())};
}

SketchEntityId Sketch::add_circle(SketchEntityId center, double radius) {
  circles_.push_back(SketchCircle{center, radius});
  return SketchEntityId{200000u + static_cast<std::uint32_t>(circles_.size())};
}

ConstraintId Sketch::add_constraint(Constraint c) {
  c.id = ConstraintId{next_constraint_++};
  constraints_.push_back(c);
  return c.id;
}

SketchPoint* Sketch::point(SketchEntityId id) {
  if (id.value == 0 || id.value > points_.size()) return nullptr;
  return &points_[id.value - 1];
}

const SketchPoint* Sketch::point(SketchEntityId id) const {
  if (id.value == 0 || id.value > points_.size()) return nullptr;
  return &points_[id.value - 1];
}

bool Sketch::is_point(SketchEntityId id) const {
  return id.value >= 1 && id.value <= points_.size();
}

std::optional<SketchLine> Sketch::line_at(std::size_t index) const {
  if (index >= lines_.size()) return std::nullopt;
  return lines_[index];
}

}  // namespace brep::sketch
