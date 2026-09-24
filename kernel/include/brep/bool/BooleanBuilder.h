#pragma once

#include "brep/bool/FaceSelector.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Model.h"
#include "brep/Topology.h"

#include <string>

namespace brep::boolean
{

struct BooleanBuildResult
{
  Body* OutputBody{nullptr};
  std::string Diagnostics;
};

/// Weld coincident vertices on an assembled shell (also rebuilds edge radial lists).
void WeldShellVertices(Shell& shell, double eps = 1e-7);

/// Weld coincident vertices/edges and pair unpartnered coedges on an assembled shell.
void StitchCopiedShell(Shell& shell, double eps = 1e-7);

/// Assemble a result body from CSG face selection (copies subgraphs into `model`).
[[nodiscard]] BooleanBuildResult BuildBooleanBody(
    Model& model, BooleanOp op, const FaceSelection& selection,
    const std::string& name);

}  // namespace brep::boolean
