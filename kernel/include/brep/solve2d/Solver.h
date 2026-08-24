#pragma once

#include "brep/param/Parameter.h"
#include "brep/sketch/Sketch.h"

#include <span>
#include <string>

namespace brep::solve2d
{

enum class SolveStatus
{
    Solved,
    UnderConstrained,
    OverConstrained,
    Failed
};

struct SolveReport
{
    SolveStatus Status{SolveStatus::Failed};
    int Dof{0};
    std::string Message;
};

class ConstraintSolver
{
public:
    /// Update Sketch point coordinates in place.
    static SolveReport Solve(sketch::Sketch& sketch,
                             std::span<const sketch::Constraint> constraints,
                             param::ParameterStore* params);
};

}  // namespace brep::solve2d
