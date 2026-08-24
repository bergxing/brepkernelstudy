#pragma once

#include "brep/Model.h"

namespace brep
{

struct BoxSpec
{
  Point3d Min{0, 0, 0};
  Point3d Max{1, 1, 1};
  double Tolerance{1e-7};
  std::string Name{"box"};
};

/// Build an axis-aligned solid box as a manifold B-Rep:
/// 1 Body -> 1 closed Shell -> 6 Faces, each with one outer Loop of 4 CoEdges.
Body* MakeBox(Model& model, const BoxSpec& spec = {});

struct SphereSpec
{
  Point3d Center{0, 0, 0};
  double Radius{1.0};
  int Slices{16};  // unused by analytic topology (compat / debug)
  int Stacks{12};  // unused by analytic topology (compat / debug)
  double Tolerance{1e-7};
  std::string Name{"sphere"};
};

/// Analytic solid sphere: dual poles + meridional seam + one SphereSurface face.
Body* MakeSphere(Model& model, const SphereSpec& spec = {});

}  // namespace brep
