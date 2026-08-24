#pragma once

#include "brep/Guid.h"
#include "brep/naming/TopologyRef.h"
#include "brep/param/Parameter.h"
#include "brep/Plane.h"
#include "brep/solve2d/Solver.h"

#include <string>
#include <vector>

namespace brep::asm_
{

struct OccurrenceId
{
    Guid Guid{};

    [[nodiscard]] friend bool operator==(const OccurrenceId& a,
                                         const OccurrenceId& b) noexcept
    {
        return a.Guid == b.Guid;
    }
};

struct Occurrence
{
    OccurrenceId Id{};
    Guid PartGuid{};
    RigidTransform Transform{};
    std::string Name;
};

enum class MateKind
{
    Coincident,
    Concentric,
    Distance,
    Angle,
    Parallel
};

struct Mate
{
    Guid Id{};
    MateKind Kind{MateKind::Coincident};
    OccurrenceId A{};
    OccurrenceId B{};
    naming::TopologyRef FaceRefA{};
    naming::TopologyRef FaceRefB{};
    param::ParameterId Dim{};
    double Aux{0.0};
};

class Assembly
{
public:
    OccurrenceId AddOccurrence(Guid partGuid, RigidTransform xf = {},
                               std::string name = {});
    Guid AddMate(Mate mate);

    [[nodiscard]] std::vector<Occurrence>& Occurrences() noexcept
    {
        return m_occurrences;
    }
    [[nodiscard]] const std::vector<Occurrence>& Occurrences() const noexcept
    {
        return m_occurrences;
    }
    [[nodiscard]] std::vector<Mate>& Mates() noexcept
    {
        return m_mates;
    }
    [[nodiscard]] const std::vector<Mate>& Mates() const noexcept
    {
        return m_mates;
    }

    Occurrence* Find(OccurrenceId id);
    const Occurrence* Find(OccurrenceId id) const;

private:
    std::vector<Occurrence> m_occurrences;
    std::vector<Mate> m_mates;
};

class MateSolver
{
public:
    /// Solve mates; updates Occurrence transforms. Distance mates read params.
    static solve2d::SolveReport Solve(Assembly& assembly,
                                      param::ParameterStore* params);
};

}  // namespace brep::asm_
