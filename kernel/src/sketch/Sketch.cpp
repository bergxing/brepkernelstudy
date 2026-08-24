#include "brep/sketch/Sketch.h"

namespace brep::sketch
{

SketchEntityId Sketch::AddPoint(Point2d p)
{
    SketchEntityId id{m_nextEntity++};
    // Point ids are indices into m_points via value encoding: store sequentially.
    // Use id.Value as 1-based index into m_points.
    m_points.push_back(SketchPoint{p, false});
    // Remap: we store entity id as index+1 matching push order for points only.
    // For mixed entities, keep parallel maps — simplify: point id == index+1 and
    // next_entity starts at 1, but lines/circles also consume ids.
    // Fix: store point_ids separately.
    (void)id;
    return SketchEntityId{static_cast<std::uint32_t>(m_points.size())};
}

SketchEntityId Sketch::AddLine(SketchEntityId p0, SketchEntityId p1)
{
    m_lines.push_back(SketchLine{p0, p1});
    return SketchEntityId{100000u + static_cast<std::uint32_t>(m_lines.size())};
}

SketchEntityId Sketch::AddCircle(SketchEntityId center, double radius)
{
    m_circles.push_back(SketchCircle{center, radius});
    return SketchEntityId{200000u + static_cast<std::uint32_t>(m_circles.size())};
}

ConstraintId Sketch::AddConstraint(Constraint c)
{
    c.Id = ConstraintId{m_nextConstraint++};
    m_constraints.push_back(c);
    return c.Id;
}

SketchPoint* Sketch::Point(SketchEntityId id)
{
    if (id.Value == 0 || id.Value > m_points.size()) return nullptr;
    return &m_points[id.Value - 1];
}

const SketchPoint* Sketch::Point(SketchEntityId id) const
{
    if (id.Value == 0 || id.Value > m_points.size()) return nullptr;
    return &m_points[id.Value - 1];
}

bool Sketch::IsPoint(SketchEntityId id) const
{
    return id.Value >= 1 && id.Value <= m_points.size();
}

std::optional<SketchLine> Sketch::LineAt(std::size_t index) const
{
    if (index >= m_lines.size()) return std::nullopt;
    return m_lines[index];
}

void Sketch::Assign(Plane frame, std::vector<SketchPoint> points,
                    std::vector<SketchLine> lines,
                    std::vector<SketchCircle> circles,
                    std::vector<Constraint> constraints,
                    std::uint32_t nextEntity, std::uint32_t nextConstraint)
{
    m_frame = std::move(frame);
    m_points = std::move(points);
    m_lines = std::move(lines);
    m_circles = std::move(circles);
    m_constraints = std::move(constraints);
    m_nextEntity = nextEntity;
    m_nextConstraint = nextConstraint;
}

}  // namespace brep::sketch
