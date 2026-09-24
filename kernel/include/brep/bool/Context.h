#pragma once

namespace brep::boolean
{

/// Tolerances for boolean evaluation (design §B5).
struct BooleanContext
{
  double fuzzy{1e-7};
  double tol_3d{1e-7};
  double tol_2d{1e-7};
  /// Viewer keeps this true. Lock tests set false so a single operand
  /// order cannot hide behind the Union/Intersect swap retry.
  bool AllowOperandSwap{true};
};

}  // namespace brep::boolean
