#pragma once

#include "brep/bool/Classify.h"
#include "brep/bool/Types.h"
#include "brep/Topology.h"

#include <vector>

namespace brep::boolean
{

/// Per-face classification relative to the other operand solid.
enum class FaceRegion : std::uint8_t
{
  In = 0,
  Out,
  On,
  Unknown,
};

[[nodiscard]] FaceRegion SolidClassToFaceRegion(SolidClass c) noexcept;

struct FaceClassification
{
  Face* TargetFace{nullptr};
  FaceRegion Region{FaceRegion::Unknown};
};

struct FaceSelection
{
  std::vector<Face*> FromA;
  std::vector<Face*> FromB;
  /// When true, faces in `FromB` must be reversed before shell assembly.
  bool ReverseB{false};
};

/// CSG face selection (design §6). Operates on classifications only — no imprint.
[[nodiscard]] FaceSelection SelectCsgFaces(
    BooleanOp op, const std::vector<FaceClassification>& aVsB,
    const std::vector<FaceClassification>& bVsA);

}  // namespace brep::boolean
