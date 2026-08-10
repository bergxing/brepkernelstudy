#pragma once

#include <cstdint>

namespace brep::boolean {

enum class BooleanOp : std::uint8_t {
  Union = 0,
  Subtract = 1,
  Intersect = 2,
};

enum class BooleanEvalMode : std::uint8_t {
  AnalyticPair = 0,
  General = 1,
};

}  // namespace brep::boolean
