#pragma once

namespace brep::boolean
{

/// Tolerances for boolean evaluation (design §B5).
struct BooleanContext
{
  double fuzzy{1e-7};
  double tol_3d{1e-7};
  double tol_2d{1e-7};
};

}  // namespace brep::boolean
