#pragma once

#include "brep/Guid.h"
#include "brep/Math.h"

#include <cstdint>
#include <optional>

namespace brep
{

enum class SnapKind : std::uint32_t
{
  None = 0,
  Endpoint = 1u << 0,
  Midpoint = 1u << 1,
  Center = 1u << 2,
  Intersection = 1u << 3,
  Perpendicular = 1u << 4,
  Nearest = 1u << 5,
  Grid = 1u << 6,       // viewer-only; kernel ignores
  Workplane = 1u << 7,  // viewer-only; kernel ignores
};

struct SnapCandidate
{
  SnapKind Kind;
  Point3d Point;
  Guid BodyGuid;
};

struct SnapQuery
{
  std::uint32_t Kinds;
  std::optional<Point3d> NearPoint;
  std::optional<Point3d> ReferencePoint;
  double Tolerance{1e-7};
};

}  // namespace brep
