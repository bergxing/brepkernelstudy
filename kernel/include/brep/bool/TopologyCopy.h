#pragma once

#include "brep/bool/FaceSelector.h"
#include "brep/Model.h"
#include "brep/Topology.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace brep::boolean
{

struct TopologyCopyContext
{
  Model& Target;
  std::unordered_map<const Point*, Point*> Points;
  std::unordered_map<const Curve*, Curve*> Curves;
  std::unordered_map<const Curve2d*, Curve2d*> Curves2d;
  std::unordered_map<const Surface*, Surface*> Surfaces;
  std::unordered_map<const Vertex*, Vertex*> Vertices;
  std::unordered_map<const Edge*, Edge*> Edges;
  std::unordered_map<const CoEdge*, CoEdge*> Coedges;
};

/// Deep-copy Face → Loop → CoEdge → Edge → Vertex → geometry into `ctx.Target`.
/// When `selectedFaces` is set, imprint coedges whose partner face is not selected
/// are replaced by the complementary boundary path from the partner loop.
[[nodiscard]] Face* CopyFaceSubgraph(
    TopologyCopyContext& ctx, const Face& src, bool reverse = false,
    const std::unordered_set<const Face*>* selectedFaces = nullptr);

/// Deep-copy an entire Body (shared edges deduplicated; partners rewired).
[[nodiscard]] Body* CopyBodySubgraph(TopologyCopyContext& ctx, const Body& src,
                                     std::string nameSuffix = "_wk");

/// Apply a rigid transform to copied geometry only (map values). Fails on
/// unknown Curve/Surface kinds so the caller can abort a half-moved body.
[[nodiscard]] bool ApplyTransformToCopied(TopologyCopyContext& ctx,
                                          const RigidTransform& t);

/// Pair copied coedges whose source coedges were partners (requires shared Edge).
void PairAllCopiedCoedgePartners(TopologyCopyContext& ctx);

struct ImprintCopyPairingReport
{
    int BothSelectedImprintCoedges{0};
    int MappedBothEnds{0};
    int PairedCopiedCoedges{0};
    int EdgeMismatch{0};
};

struct SeamExpansionReport
{
    int PartnerUnselected{0};
    int Expanded{0};
    int PreservedForSelectedBoundary{0};
    int CopiedImprintFallback{0};
    std::vector<std::string> FallbackImprintNames;
};

[[nodiscard]] ImprintCopyPairingReport ReportImprintCopyPairing(
    const FaceSelection& selection, const TopologyCopyContext& ctx);

[[nodiscard]] SeamExpansionReport ReportSeamExpansion(const FaceSelection& selection);

}  // namespace brep::boolean
