#pragma once

#include "brep/model.hpp"

namespace brep {

struct BoxSpec {
  Point3d min{0, 0, 0};
  Point3d max{1, 1, 1};
  double tolerance{1e-7};
  std::string name{"box"};
};

/// Build an axis-aligned solid box as a manifold B-Rep:
/// 1 Body -> 1 closed Shell -> 6 Faces, each with one outer Loop of 4 CoEdges.
Body* make_box(Model& model, const BoxSpec& spec = {});

struct SphereSpec {
  Point3d center{0, 0, 0};
  double radius{1.0};
  int slices{16};  // unused by analytic topology (compat / debug)
  int stacks{12};  // unused by analytic topology (compat / debug)
  double tolerance{1e-7};
  std::string name{"sphere"};
};

/// Analytic solid sphere: dual poles + meridional seam + one SphereSurface face.
Body* make_sphere(Model& model, const SphereSpec& spec = {});

}  // namespace brep
