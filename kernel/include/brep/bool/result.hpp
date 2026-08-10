#pragma once

#include "brep/bool/types.hpp"
#include "brep/topology.hpp"

#include <string>

namespace brep::boolean {

struct BooleanResult {
  Body* body{nullptr};  // owned by Model/Part when non-null
  BooleanEvalMode mode{BooleanEvalMode::General};
  std::string diagnostics;

  [[nodiscard]] bool ok() const noexcept { return body != nullptr; }
};

}  // namespace brep::boolean
