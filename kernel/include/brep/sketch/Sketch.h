#pragma once

#include "brep/Math.h"
#include "brep/param/Parameter.h"
#include "brep/Plane.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace brep::sketch
{

struct SketchEntityId
{
    std::uint32_t Value{0};

    [[nodiscard]] friend bool operator==(SketchEntityId a,
                                         SketchEntityId b) noexcept
    {
        return a.Value == b.Value;
    }
};

enum class EntityKind
{
    Point,
    Line,
    Circle,
    Arc
};

struct SketchPoint
{
    Point2d P{};
    bool Fixed{false};
};

struct SketchLine
{
    SketchEntityId P0{};
    SketchEntityId P1{};
};

struct SketchCircle
{
    SketchEntityId Center{};
    double Radius{1.0};
};

enum class ConstraintKind
{
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

struct ConstraintId
{
    std::uint32_t Value{0};
};

struct Constraint
{
    ConstraintId Id{};
    ConstraintKind Kind{ConstraintKind::Fixed};
    SketchEntityId A{};
    SketchEntityId B{};
    param::ParameterId Dim{};
    double Aux{0.0};
};

class Sketch
{
public:
    SketchEntityId AddPoint(Point2d p);
    SketchEntityId AddLine(SketchEntityId p0, SketchEntityId p1);
    SketchEntityId AddCircle(SketchEntityId center, double radius);

    ConstraintId AddConstraint(Constraint c);

    [[nodiscard]] Plane Frame() const
    {
        return m_frame;
    }
    void SetFrame(Plane frame)
    {
        m_frame = std::move(frame);
    }

    [[nodiscard]] SketchPoint* Point(SketchEntityId id);
    [[nodiscard]] const SketchPoint* Point(SketchEntityId id) const;
    [[nodiscard]] const std::vector<SketchPoint>& Points() const
    {
        return m_points;
    }
    [[nodiscard]] const std::vector<SketchLine>& Lines() const
    {
        return m_lines;
    }
    [[nodiscard]] std::vector<SketchCircle>& Circles()
    {
        return m_circles;
    }
    [[nodiscard]] const std::vector<SketchCircle>& Circles() const
    {
        return m_circles;
    }
    [[nodiscard]] const std::vector<Constraint>& Constraints() const
    {
        return m_constraints;
    }
    std::vector<Constraint>& Constraints()
    {
        return m_constraints;
    }

    [[nodiscard]] bool IsPoint(SketchEntityId id) const;
    [[nodiscard]] std::optional<SketchLine> LineAt(std::size_t index) const;

    /// Replace sketch contents (document load).
    void Assign(Plane frame, std::vector<SketchPoint> points,
                std::vector<SketchLine> lines, std::vector<SketchCircle> circles,
                std::vector<Constraint> constraints, std::uint32_t nextEntity,
                std::uint32_t nextConstraint);

    [[nodiscard]] std::uint32_t NextEntity() const noexcept
    {
        return m_nextEntity;
    }
    [[nodiscard]] std::uint32_t NextConstraint() const noexcept
    {
        return m_nextConstraint;
    }

private:
    Plane m_frame{Plane::XzYUp()};
    std::vector<SketchPoint> m_points;
    std::vector<SketchLine> m_lines;
    std::vector<SketchCircle> m_circles;
    std::vector<Constraint> m_constraints;
    std::uint32_t m_nextEntity{1};
    std::uint32_t m_nextConstraint{1};
};

}  // namespace brep::sketch
