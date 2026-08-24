#pragma once

#include "brep/bool/Types.h"
#include "brep/Topology.h"

#include <string>

namespace brep::boolean
{

struct BooleanResult
{
  Body* OutputBody{nullptr};  // owned by Model/Part when non-null
  BooleanEvalMode Mode{BooleanEvalMode::General};
  std::string Diagnostics;

  [[nodiscard]] bool Ok() const noexcept
  {
    return OutputBody != nullptr;
  }
};

}  // namespace brep::boolean
