#pragma once

#include <cstdint>

namespace brep::boolean
{

enum class BooleanOp : std::uint8_t
{
  Union = 0,
  Subtract = 1,
  Intersect = 2,
};

enum class BooleanEvalMode : std::uint8_t
{
  General = 0,
};

}  // namespace brep::boolean
