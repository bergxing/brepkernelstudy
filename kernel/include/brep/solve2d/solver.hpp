#pragma once

#include "brep/param/parameter.hpp"
#include "brep/sketch/sketch.hpp"

#include <span>
#include <string>

namespace brep::solve2d {

enum class SolveStatus { Solved, UnderConstrained, OverConstrained, Failed };

struct SolveReport {
  SolveStatus status{SolveStatus::Failed};
  int dof{0};
  std::string message;
};

class ConstraintSolver {
 public:
  /// Update Sketch point coordinates in place.
  static SolveReport solve(sketch::Sketch& sketch,
                           std::span<const sketch::Constraint> constraints,
                           param::ParameterStore* params);
};

}  // namespace brep::solve2d
