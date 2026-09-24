#include "brep/bool/TopologyCopy.h"

#include "brep/Geometry.h"
#include "brep/internal/Fuzzy.h"

#include <array>
#include <numbers>

namespace brep::boolean
{
namespace
{

Point* CopyPoint(TopologyCopyContext& ctx, const Point* src)
{
  if (!src)
  {
    return nullptr;
  }
  if (auto it = ctx.Points.find(src); it != ctx.Points.end())
  {
    return it->second;
  }
  Point* copy = ctx.Target.MakePoint(src->Xyz(), src->Name + "_cp");
  ctx.Points.emplace(src, copy);
  return copy;
}

Curve* CopyCurve(TopologyCopyContext& ctx, const Curve* src)
{
  if (!src)
  {
    return nullptr;
  }
  if (auto it = ctx.Curves.find(src); it != ctx.Curves.end())
  {
    return it->second;
  }
  Curve* copy = nullptr;
  switch (src->Kind())
  {
    case CurveKind::Line:
    {
      const auto& line = static_cast<const LineCurve&>(*src);
      copy = ctx.Target.MakeLine(line.Origin(),
                                 line.Origin() + line.Direction() * line.Length(),
                                 "line_cp");
      static_cast<LineCurve*>(copy)->SetLength(line.Length());
      break;
    }
    case CurveKind::Circle:
    {
      const auto& circle = static_cast<const CircleCurve&>(*src);
      copy = ctx.Target.MakeCircle(circle.Center(), circle.Normal(), circle.Radius(),
                                   "circle_cp");
      break;
    }
    case CurveKind::Bezier:
    {
      const auto& bezier = static_cast<const BezierCurve&>(*src);
      copy = ctx.Target.MakeBezier(bezier.Cvs(), bezier.Weights(),
                                   "bezier_cp");
      break;
    }
    case CurveKind::Nurbs:
    {
      const auto& nurbs = static_cast<const NurbsCurve&>(*src);
      copy = ctx.Target.MakeNurbs(nurbs.Cvs(), nurbs.Weights(), nurbs.Knots(),
                                  "nurbs_cp");
      break;
    }
    default:
      return nullptr;
  }
  ctx.Curves.emplace(src, copy);
  return copy;
}

Curve2d* CopyCurve2d(TopologyCopyContext& ctx, const Curve2d* src)
{
  if (!src)
  {
    return nullptr;
  }
  if (auto it = ctx.Curves2d.find(src); it != ctx.Curves2d.end())
  {
    return it->second;
  }
  Curve2d* copy = nullptr;
  if (const auto* polyline = dynamic_cast<const PolylineCurve2d*>(src))
  {
    copy = ctx.Target.MakePolyline2d(polyline->Points());
  }
  else
  {
    copy = ctx.Target.MakeLine2d(src->Eval(0.0), src->Eval(1.0));
  }
  ctx.Curves2d.emplace(src, copy);
  return copy;
}

Surface* CopySurface(TopologyCopyContext& ctx, const Surface* src)
{
  if (!src)
  {
    return nullptr;
  }
  if (auto it = ctx.Surfaces.find(src); it != ctx.Surfaces.end())
  {
    return it->second;
  }
  Surface* copy = nullptr;
  switch (src->Kind())
  {
    case SurfaceKind::Plane:
    {
      const auto& plane = static_cast<const PlaneSurface&>(*src);
      copy = ctx.Target.MakePlane(plane.Origin(), plane.UAxis(), plane.VAxis(),
                                  "plane_cp");
      break;
    }
    case SurfaceKind::Sphere:
    {
      const auto& sphere = static_cast<const SphereSurface&>(*src);
      copy = ctx.Target.MakeSphereSurface(sphere.Center(), sphere.Radius(),
                                          "sphere_cp");
      break;
    }
    case SurfaceKind::Cylinder:
    {
      const auto& cylinder = static_cast<const CylinderSurface&>(*src);
      copy = ctx.Target.MakeCylinderSurface(cylinder.Origin(), cylinder.Axis(),
                                            cylinder.Radius(), "cylinder_cp");
      break;
    }
    default:
      return nullptr;
  }
  ctx.Surfaces.emplace(src, copy);
  return copy;
}

Vertex* CopyVertex(TopologyCopyContext& ctx, const Vertex* src)
{
  if (!src)
  {
    return nullptr;
  }
  if (auto it = ctx.Vertices.find(src); it != ctx.Vertices.end())
  {
    return it->second;
  }
  Vertex* copy =
      ctx.Target.MakeVertex(CopyPoint(ctx, src->Point), src->Tolerance, src->Name + "_cp");
  ctx.Vertices.emplace(src, copy);
  return copy;
}

Edge* CopyEdge(TopologyCopyContext& ctx, const Edge* src)
{
  if (!src)
  {
    return nullptr;
  }
  if (auto it = ctx.Edges.find(src); it != ctx.Edges.end())
  {
    return it->second;
  }
  Edge* copy = ctx.Target.MakeEdge(CopyCurve(ctx, src->Curve), CopyVertex(ctx, src->V0),
                                   CopyVertex(ctx, src->V1), src->T0, src->T1,
                                   src->Tolerance, src->Name + "_cp");
  Model::AttachEdgeToVertices(copy);
  ctx.Edges.emplace(src, copy);
  return copy;
}

[[nodiscard]] bool IsImprintCoedge(const CoEdge& coedge)
{
  return coedge.Name.find("_imprint") != std::string::npos;
}

[[nodiscard]] bool SameCoedgeEndpoints(const CoEdge& a, const CoEdge& b,
                                       double eps)
{
  if (a.From() == nullptr || a.To() == nullptr || b.From() == nullptr ||
      b.To() == nullptr)
  {
    return false;
  }
  const auto close = [eps](const Vertex& lhs, const Vertex& rhs)
  {
    return (lhs.Position() - rhs.Position()).norm() <= eps;
  };
  return (close(*a.From(), *b.From()) && close(*a.To(), *b.To())) ||
         (close(*a.From(), *b.To()) && close(*a.To(), *b.From()));
}

[[nodiscard]] bool SelectedFaceProvidesBoundary(
    const CoEdge& seam, const std::unordered_set<const Face*>& selectedFaces,
    double eps)
{
  const Face* seamFace = seam.GetFace();
  for (const Face* face : selectedFaces)
  {
    if (face == nullptr || face == seamFace)
    {
      continue;
    }
    for (const Loop* loop : face->Loops)
    {
      if (loop == nullptr)
      {
        continue;
      }
      bool found = false;
      loop->ForEachCoedge([&](const CoEdge& candidate)
      {
        if (!found && SameCoedgeEndpoints(seam, candidate, eps))
        {
          found = true;
        }
      });
      if (found)
      {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] CoEdge* FindCoedgeFrom(Loop& loop, const Vertex& vertex)
{
  if (loop.First == nullptr)
  {
    return nullptr;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 1024U)
    {
      return nullptr;
    }
    if (coedge->From() == &vertex)
    {
      return coedge;
    }
    coedge = coedge->Next;
  } while (coedge != nullptr && coedge != loop.First);
  return nullptr;
}

[[nodiscard]] CoEdge* FindCoedgeTo(Loop& loop, const Vertex& vertex)
{
  if (loop.First == nullptr)
  {
    return nullptr;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 1024U)
    {
      return nullptr;
    }
    if (coedge->To() == &vertex)
    {
      return coedge;
    }
    coedge = coedge->Next;
  } while (coedge != nullptr && coedge != loop.First);
  return nullptr;
}

[[nodiscard]] CoEdge* FindCoedgeDeparting(Loop& loop, const Vertex& vertex)
{
  if (CoEdge* coedge = FindCoedgeFrom(loop, vertex))
  {
    return coedge;
  }
  if (CoEdge* arrive = FindCoedgeTo(loop, vertex))
  {
    return arrive->Next;
  }
  return nullptr;
}

[[nodiscard]] std::vector<const CoEdge*> CollectLoopPathConst(const Loop& loop,
                                                                const Vertex& from,
                                                                const Vertex& to)
{
  std::vector<const CoEdge*> path;
  Loop& mutableLoop = const_cast<Loop&>(loop);
  CoEdge* start = FindCoedgeDeparting(mutableLoop, from);
  if (start == nullptr)
  {
    return path;
  }
  CoEdge* cur = start;
  std::size_t guard = 0;
  do
  {
    if (++guard > 1024U)
    {
      return {};
    }
    path.push_back(cur);
    if (cur->To() == &to)
    {
      return path;
    }
    if (cur->Next != nullptr && cur->Next->From() == &to)
    {
      return path;
    }
    cur = cur->Next;
  } while (cur != nullptr && cur != start);
  return {};
}

[[nodiscard]] std::vector<const CoEdge*> CollectLoopPathConstReverse(
    const Loop& loop, const Vertex& from, const Vertex& to)
{
  std::vector<const CoEdge*> path;
  Loop& mutableLoop = const_cast<Loop&>(loop);
  CoEdge* start = FindCoedgeDeparting(mutableLoop, from);
  if (start == nullptr)
  {
    return path;
  }
  CoEdge* cur = start;
  std::size_t guard = 0;
  do
  {
    if (++guard > 1024U)
    {
      return {};
    }
    path.push_back(cur);
    if (cur->To() == &to)
    {
      return path;
    }
    if (cur->Next != nullptr && cur->Next->From() == &to)
    {
      return path;
    }
    cur = cur->Prev;
  } while (cur != nullptr && cur != start);
  return {};
}

[[nodiscard]] bool IsBlockingSeamImprint(const CoEdge& coedge, const CoEdge* excludeImprint,
                                       const std::unordered_set<const Face*>* selectedFaces)
{
  if (&coedge == excludeImprint || !IsImprintCoedge(coedge))
  {
    return false;
  }
  if (selectedFaces == nullptr)
  {
    return true;
  }
  const Face* partnerFace =
      coedge.Partner != nullptr ? coedge.Partner->GetFace() : nullptr;
  return partnerFace == nullptr || selectedFaces->count(partnerFace) == 0U;
}

[[nodiscard]] bool PathUsableForSeam(const std::vector<const CoEdge*>& path,
                                     const CoEdge* excludeImprint,
                                     const std::unordered_set<const Face*>* selectedFaces)
{
  if (path.empty())
  {
    return false;
  }
  for (const CoEdge* coedge : path)
  {
    if (coedge == excludeImprint)
    {
      return false;
    }
    if (coedge != nullptr && IsBlockingSeamImprint(*coedge, excludeImprint, selectedFaces))
    {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::vector<const CoEdge*> CollectComplementaryBoundaryPath(
    const Loop& loop, const Vertex& from, const Vertex& to,
    const CoEdge* excludeImprint,
    const std::unordered_set<const Face*>* selectedFaces)
{
  const std::vector<const CoEdge*> forward = CollectLoopPathConst(loop, from, to);
  const std::vector<const CoEdge*> backward = CollectLoopPathConst(loop, to, from);
  const std::vector<const CoEdge*> forwardRev =
      CollectLoopPathConstReverse(loop, from, to);
  const std::vector<const CoEdge*> backwardRev =
      CollectLoopPathConstReverse(loop, to, from);
  if (PathUsableForSeam(backward, excludeImprint, selectedFaces))
  {
    return backward;
  }
  if (PathUsableForSeam(forward, excludeImprint, selectedFaces))
  {
    return forward;
  }
  if (PathUsableForSeam(backwardRev, excludeImprint, selectedFaces))
  {
    return backwardRev;
  }
  if (PathUsableForSeam(forwardRev, excludeImprint, selectedFaces))
  {
    return forwardRev;
  }
  return {};
}

[[nodiscard]] std::vector<const CoEdge*>
CollectImprintComplementArc(const Loop& loop, const CoEdge& excludeImprint,
                            const std::unordered_set<const Face*>* selectedFaces);

[[nodiscard]] bool AppendSeamLoopSegment(
    std::vector<const CoEdge*>& patch, const Loop& loop, const CoEdge& imprint,
    const std::unordered_set<const Face*>* selectedFaces, int depth,
    std::unordered_set<const CoEdge*>& activeImprints)
{
  if (depth > 8 || activeImprints.count(&imprint) != 0U)
  {
    return false;
  }
  activeImprints.insert(&imprint);
  if (imprint.Next == nullptr || imprint.Prev == nullptr || imprint.Loop != &loop)
  {
    activeImprints.erase(&imprint);
    return false;
  }
  CoEdge* walk = imprint.Next;
  std::size_t guard = 0;
  while (walk != &imprint)
  {
    if (++guard > 1024U)
    {
      activeImprints.erase(&imprint);
      return false;
    }
    if (IsBlockingSeamImprint(*walk, &imprint, selectedFaces))
    {
      std::vector<const CoEdge*> nested;
      if (walk->Partner != nullptr && walk->Partner->Loop != nullptr)
      {
        nested = CollectImprintComplementArc(*walk->Partner->Loop, *walk->Partner,
                                             selectedFaces);
        if (nested.empty())
        {
          nested = CollectComplementaryBoundaryPath(*walk->Partner->Loop, *walk->From(),
                                                    *walk->To(), walk->Partner, selectedFaces);
        }
        if (nested.empty())
        {
          nested = CollectComplementaryBoundaryPath(*walk->Partner->Loop, *walk->To(),
                                                    *walk->From(), walk->Partner, selectedFaces);
        }
      }
      if (nested.empty() &&
          !AppendSeamLoopSegment(nested, loop, *walk, selectedFaces, depth + 1, activeImprints))
      {
        activeImprints.erase(&imprint);
        return false;
      }
      patch.insert(patch.end(), nested.begin(), nested.end());
      walk = walk->Next;
      continue;
    }
    patch.push_back(walk);
    walk = walk->Next;
  }
  activeImprints.erase(&imprint);
  return !patch.empty();
}

[[nodiscard]] std::vector<const CoEdge*>
CollectImprintComplementArc(const Loop& loop, const CoEdge& excludeImprint,
                            const std::unordered_set<const Face*>* selectedFaces)
{
  std::vector<const CoEdge*> patch;
  std::unordered_set<const CoEdge*> activeImprints;
  if (!AppendSeamLoopSegment(patch, loop, excludeImprint, selectedFaces, 0, activeImprints))
  {
    patch.clear();
  }
  return patch;
}

struct CoedgeCopySpec
{
  const CoEdge* Source{nullptr};
  bool ReverseSense{false};
};

[[nodiscard]] Vertex* EffectiveCoedgeFrom(const CoEdge& coedge, bool reverseSense)
{
  return reverseSense ? coedge.To() : coedge.From();
}

[[nodiscard]] Vertex* EffectiveCoedgeTo(const CoEdge& coedge, bool reverseSense)
{
  return reverseSense ? coedge.From() : coedge.To();
}

[[nodiscard]] bool PatchConnectsImprint(const CoEdge& imprint,
                                        const std::vector<const CoEdge*>& patch,
                                        bool reversePatchSense)
{
  if (patch.empty() || imprint.From() == nullptr || imprint.To() == nullptr)
  {
    return false;
  }
  if (EffectiveCoedgeFrom(*patch.front(), reversePatchSense) != imprint.From())
  {
    return false;
  }
  for (std::size_t i = 0; i + 1U < patch.size(); ++i)
  {
    if (patch[i] == nullptr || patch[i + 1U] == nullptr)
    {
      return false;
    }
    if (EffectiveCoedgeTo(*patch[i], reversePatchSense) !=
        EffectiveCoedgeFrom(*patch[i + 1U], reversePatchSense))
    {
      return false;
    }
  }
  return EffectiveCoedgeTo(*patch.back(), reversePatchSense) == imprint.To();
}

struct SeamPatchResult
{
  std::vector<const CoEdge*> Patch;
  bool ReversePatchSense{false};
  bool Ok{false};
};

[[nodiscard]] SeamPatchResult ResolveSeamPatch(const Loop& loop, const CoEdge& imprint,
                                               const std::unordered_set<const Face*>* selectedFaces)
{
  (void)loop;
  SeamPatchResult result;
  if (imprint.From() == nullptr || imprint.To() == nullptr || imprint.Partner == nullptr ||
      imprint.Partner->Loop == nullptr)
  {
    return result;
  }

  const auto finalize = [&](std::vector<const CoEdge*> patch) -> bool
  {
    if (patch.empty())
    {
      return false;
    }
    bool reversePatchSense = false;
    if (!PatchConnectsImprint(imprint, patch, false))
    {
      if (PatchConnectsImprint(imprint, patch, true))
      {
        reversePatchSense = true;
      }
      else
      {
        std::reverse(patch.begin(), patch.end());
        if (PatchConnectsImprint(imprint, patch, false))
        {
          reversePatchSense = false;
        }
        else if (PatchConnectsImprint(imprint, patch, true))
        {
          reversePatchSense = true;
        }
        else
        {
          return false;
        }
      }
    }
    result.Patch = std::move(patch);
    result.ReversePatchSense = reversePatchSense;
    result.Ok = true;
    return true;
  };

  Loop& partnerLoop = *imprint.Partner->Loop;
  if (finalize(CollectImprintComplementArc(partnerLoop, *imprint.Partner, selectedFaces)))
  {
    return result;
  }

  const Vertex* from = imprint.From();
  const Vertex* to = imprint.To();
  const std::array<std::pair<const Vertex*, const Vertex*>, 2> endpoints = {
      std::pair{from, to},
      std::pair{to, from},
  };
  for (const auto& [pathFrom, pathTo] : endpoints)
  {
    if (finalize(CollectComplementaryBoundaryPath(partnerLoop, *pathFrom, *pathTo,
                                                  imprint.Partner, selectedFaces)))
    {
      return result;
    }
  }

  return result;
}

[[nodiscard]] std::vector<const CoEdge*> CollectComplementaryBoundaryPath(
    const Loop& loop, const Vertex& from, const Vertex& to,
    const CoEdge* excludeImprint = nullptr)
{
  return CollectComplementaryBoundaryPath(loop, from, to, excludeImprint, nullptr);
}

[[nodiscard]] bool SpecsFormClosedLoop(const std::vector<CoedgeCopySpec>& specs)
{
  if (specs.size() < 3U)
  {
    return false;
  }
  for (std::size_t i = 0; i < specs.size(); ++i)
  {
    const CoedgeCopySpec& cur = specs[i];
    const CoedgeCopySpec& next = specs[(i + 1U) % specs.size()];
    if (cur.Source == nullptr || next.Source == nullptr)
    {
      return false;
    }
    if (EffectiveCoedgeTo(*cur.Source, cur.ReverseSense) !=
        EffectiveCoedgeFrom(*next.Source, next.ReverseSense))
    {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool ShouldCopyImprintBoundaryCoedge(
    const CoEdge& coedge, const std::unordered_set<const Face*>* selectedFaces)
{
  if (selectedFaces == nullptr)
  {
    return true;
  }
  if (coedge.From() != nullptr && coedge.To() != nullptr &&
      coedge.From() == coedge.To())
  {
    return false;
  }
  if (IsImprintCoedge(coedge))
  {
    return true;
  }
  if (coedge.Partner == nullptr)
  {
    return true;
  }
  const Face* partnerFace = coedge.Partner->GetFace();
  if (partnerFace == nullptr || selectedFaces->count(partnerFace) != 0U)
  {
    return true;
  }
  return SelectedFaceProvidesBoundary(coedge, *selectedFaces, 1e-7);
}

[[nodiscard]] std::vector<CoedgeCopySpec> CollectLoopCoedgesForCopy(
    const Loop& loop, const std::unordered_set<const Face*>* selectedFaces);

[[nodiscard]] std::vector<CoedgeCopySpec>
CollectLoopCoedgesLiteral(const Loop& loop,
                          const std::unordered_set<const Face*>* selectedFaces)
{
  std::vector<CoedgeCopySpec> ordered;
  if (loop.First == nullptr)
  {
    return ordered;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 1024U)
    {
      break;
    }
    if (ShouldCopyImprintBoundaryCoedge(*coedge, selectedFaces))
    {
      ordered.push_back(CoedgeCopySpec{coedge, false});
    }
    coedge = coedge->Next;
  } while (coedge != nullptr && coedge != loop.First);
  return ordered;
}

[[nodiscard]] std::vector<CoedgeCopySpec>
CollectLoopCoedgesLiteral(const Loop& loop)
{
  return CollectLoopCoedgesLiteral(loop, nullptr);
}

[[nodiscard]] std::vector<CoedgeCopySpec>
CollectLoopCoedgesForCopy(const Loop& loop,
                          const std::unordered_set<const Face*>* selectedFaces)
{
  std::vector<CoedgeCopySpec> ordered;
  std::unordered_set<const CoEdge*> emitted;
  if (loop.First == nullptr)
  {
    return ordered;
  }
  CoEdge* coedge = loop.First;
  std::size_t guard = 0;
  do
  {
    if (++guard > 1024U)
    {
      break;
    }
    if (emitted.count(coedge) != 0U)
    {
      coedge = coedge->Next;
      continue;
    }
    if (selectedFaces != nullptr && IsImprintCoedge(*coedge) && coedge->From() != nullptr &&
        coedge->To() != nullptr)
    {
      const Face* partnerFace =
          coedge->Partner != nullptr ? coedge->Partner->GetFace() : nullptr;
      const bool partnerUnselected =
          partnerFace == nullptr || selectedFaces->count(partnerFace) == 0U;
      if (partnerUnselected &&
          !SelectedFaceProvidesBoundary(*coedge, *selectedFaces, 1e-7))
      {
        const SeamPatchResult seam = ResolveSeamPatch(loop, *coedge, selectedFaces);
        if (seam.Ok)
        {
          for (const CoEdge* patchCoedge : seam.Patch)
          {
            if (patchCoedge != nullptr && emitted.count(patchCoedge) == 0U)
            {
              ordered.push_back(CoedgeCopySpec{patchCoedge, seam.ReversePatchSense});
              emitted.insert(patchCoedge);
            }
          }
          emitted.insert(coedge);
          coedge = coedge->Next;
          continue;
        }
      }
    }
    ordered.push_back(CoedgeCopySpec{coedge, false});
    emitted.insert(coedge);
    coedge = coedge->Next;
  } while (coedge != nullptr && coedge != loop.First);
  if (selectedFaces != nullptr && !SpecsFormClosedLoop(ordered))
  {
    return CollectLoopCoedgesLiteral(loop);
  }
  return ordered;
}

CoEdge* CopyCoedge(TopologyCopyContext& ctx, const CoEdge& coedge, bool reverse)
{
  Orientation coedgeSense = coedge.Sense;
  if (reverse)
  {
    coedgeSense = opposite(coedgeSense);
  }
  CoEdge* newCoedge = ctx.Target.MakeCoedge(
      CopyEdge(ctx, coedge.Edge), coedgeSense, CopyCurve2d(ctx, coedge.Pcurve),
      coedge.Name + "_cp");
  ctx.Coedges.emplace(&coedge, newCoedge);
  return newCoedge;
}

}  // namespace

void TryPairCopiedPartner(TopologyCopyContext& ctx, const CoEdge& src, CoEdge* dst)
{
  if (dst == nullptr || src.Partner == nullptr)
  {
    return;
  }
  if (src.Name.find("_complement_coedge") != std::string::npos ||
      src.Partner->Name.find("_complement_coedge") != std::string::npos)
  {
    return;
  }
  const auto partnerIt = ctx.Coedges.find(src.Partner);
  if (partnerIt == ctx.Coedges.end() || partnerIt->second == nullptr)
  {
    return;
  }
  CoEdge* partnerDst = partnerIt->second;
  if (dst->Partner != nullptr || partnerDst->Partner != nullptr)
  {
    return;
  }
  if (dst->Edge == nullptr || partnerDst->Edge == nullptr || dst->Edge != partnerDst->Edge)
  {
    return;
  }
  Model::PairPartners(dst, partnerDst);
}

void PairAllCopiedCoedgePartners(TopologyCopyContext& ctx)
{
  for (const auto& [srcCoedge, dstCoedge] : ctx.Coedges)
  {
    if (srcCoedge == nullptr || dstCoedge == nullptr || srcCoedge->Partner == nullptr)
    {
      continue;
    }
    TryPairCopiedPartner(ctx, *srcCoedge, dstCoedge);
  }
}

ImprintCopyPairingReport ReportImprintCopyPairing(const FaceSelection& selection,
                                                  const TopologyCopyContext& ctx)
{
  ImprintCopyPairingReport report;
  std::unordered_set<const Face*> selectedFaces;
  for (Face* face : selection.FromA)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }
  for (Face* face : selection.FromB)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }

  const auto scanFace = [&](Face* face)
  {
    if (face == nullptr || selectedFaces.count(face) == 0U)
    {
      return;
    }
    for (Loop* loop : face->Loops)
    {
      if (loop == nullptr)
      {
        continue;
      }
      loop->ForEachCoedge([&](CoEdge& coedge)
      {
        if (coedge.Name.find("_imprint") == std::string::npos || coedge.Partner == nullptr)
        {
          return;
        }
        const Face* partnerFace = coedge.Partner->GetFace();
        if (partnerFace == nullptr || selectedFaces.count(partnerFace) == 0U)
        {
          return;
        }
        ++report.BothSelectedImprintCoedges;
        const auto dstIt = ctx.Coedges.find(&coedge);
        const auto partnerIt = ctx.Coedges.find(coedge.Partner);
        if (dstIt == ctx.Coedges.end() || partnerIt == ctx.Coedges.end())
        {
          return;
        }
        ++report.MappedBothEnds;
        if (dstIt->second->Edge != partnerIt->second->Edge)
        {
          ++report.EdgeMismatch;
          return;
        }
        if (dstIt->second->Partner != nullptr && partnerIt->second->Partner != nullptr)
        {
          ++report.PairedCopiedCoedges;
        }
      });
    }
  };

  for (Face* face : selection.FromA)
  {
    scanFace(face);
  }
  for (Face* face : selection.FromB)
  {
    scanFace(face);
  }
  return report;
}

SeamExpansionReport ReportSeamExpansion(const FaceSelection& selection)
{
  SeamExpansionReport report;
  std::unordered_set<const Face*> selectedFaces;
  for (Face* face : selection.FromA)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }
  for (Face* face : selection.FromB)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }

  const auto scanFace = [&](Face* face)
  {
    if (face == nullptr || selectedFaces.count(face) == 0U)
    {
      return;
    }
    for (Loop* loop : face->Loops)
    {
      if (loop == nullptr)
      {
        continue;
      }
      loop->ForEachCoedge([&](CoEdge& coedge)
      {
        if (!IsImprintCoedge(coedge) || coedge.Partner == nullptr)
        {
          return;
        }
        const Face* partnerFace = coedge.Partner->GetFace();
        if (partnerFace != nullptr && selectedFaces.count(partnerFace) != 0U)
        {
          return;
        }
        ++report.PartnerUnselected;
        if (SelectedFaceProvidesBoundary(coedge, selectedFaces, 1e-7))
        {
          ++report.PreservedForSelectedBoundary;
        }
        else if (ResolveSeamPatch(*loop, coedge, &selectedFaces).Ok)
        {
          ++report.Expanded;
        }
        else
        {
          ++report.CopiedImprintFallback;
          report.FallbackImprintNames.push_back(coedge.Name);
        }
      });
    }
  };

  for (Face* face : selection.FromA)
  {
    scanFace(face);
  }
  for (Face* face : selection.FromB)
  {
    scanFace(face);
  }
  return report;
}

[[nodiscard]] bool IsImprintFragmentFace(const Face& face)
{
  return face.Name.find("_split") != std::string::npos ||
         face.Name.find("_sphere_imprint_patch") != std::string::npos ||
         face.Name.find("_circle_imprint_disk") != std::string::npos ||
         face.Name.find("_circle_imprint_cap") != std::string::npos;
}

[[nodiscard]] bool ComplementPartnerSelected(
    const Loop& loop, const std::unordered_set<const Face*>& selectedFaces)
{
  bool selected = false;
  loop.ForEachCoedge([&](const CoEdge& coedge)
  {
    if (selected || coedge.Partner == nullptr)
    {
      return;
    }
    const Face* partnerFace = coedge.Partner->GetFace();
    if (partnerFace != nullptr && selectedFaces.count(partnerFace) != 0U)
    {
      selected = true;
    }
  });
  return selected;
}

Face* CopyFaceSubgraph(TopologyCopyContext& ctx, const Face& src, bool reverse,
                       const std::unordered_set<const Face*>* selectedFaces)
{
  Surface* surface = CopySurface(ctx, src.Surface);
  if (!surface)
  {
    return nullptr;
  }
  Orientation sense = src.Sense;
  if (reverse)
  {
    sense = opposite(sense);
  }
  Face* face = ctx.Target.MakeFace(surface, sense, src.Name + "_cp");

  for (const Loop* loop : src.Loops)
  {
    if (!loop || !loop->First)
    {
      continue;
    }
    // Skip the remainder hole when the imprint patch is also copied; keep
    // it when CSG discarded the patch (sphere-minus-solid / sphere union).
    if (!reverse &&
        loop->Name.find("_complement_inner") != std::string::npos &&
        (selectedFaces == nullptr ||
         ComplementPartnerSelected(*loop, *selectedFaces)))
    {
      continue;
    }
    Loop* newLoop = ctx.Target.MakeLoop(face, loop->Type, loop->Name + "_cp");
    std::vector<CoedgeCopySpec> sourceCoedges =
        selectedFaces != nullptr && IsImprintFragmentFace(src)
            ? CollectLoopCoedgesLiteral(*loop, selectedFaces)
            : CollectLoopCoedgesForCopy(*loop, selectedFaces);
    if (reverse)
    {
      std::reverse(sourceCoedges.begin(), sourceCoedges.end());
    }
    std::vector<CoEdge*> newCoedges;
    newCoedges.reserve(sourceCoedges.size());
    for (const CoedgeCopySpec& spec : sourceCoedges)
    {
      if (spec.Source == nullptr)
      {
        continue;
      }
      CoEdge* copied = CopyCoedge(ctx, *spec.Source, reverse ^ spec.ReverseSense);
      TryPairCopiedPartner(ctx, *spec.Source, copied);
      newCoedges.push_back(copied);
    }
    if (!newCoedges.empty())
    {
      Model::LinkLoop(newLoop, newCoedges);
    }
  }
  return face;
}

Body* CopyBodySubgraph(TopologyCopyContext& ctx, const Body& src,
                       std::string nameSuffix)
{
  Body* body = ctx.Target.MakeBody(src.Type, src.Name + nameSuffix);
  for (Shell* shell : src.Shells)
  {
    if (!shell)
    {
      continue;
    }
    Shell* newShell =
        ctx.Target.MakeShell(shell->Closed, shell->Name + nameSuffix);
    body->Shells.push_back(newShell);
    for (Face* face : shell->Faces)
    {
      if (!face)
      {
        continue;
      }
      Face* newFace = CopyFaceSubgraph(ctx, *face);
      if (newFace)
      {
        newShell->Faces.push_back(newFace);
      }
    }
  }

  for (Edge* edge : src.WireEdges)
  {
    if (!edge)
    {
      continue;
    }
    Edge* newEdge = CopyEdge(ctx, edge);
    if (newEdge)
    {
      body->WireEdges.push_back(newEdge);
    }
  }

  PairAllCopiedCoedgePartners(ctx);
  return body;
}

bool ApplyTransformToCopied(TopologyCopyContext& ctx, const RigidTransform& t)
{
    for (auto& [src, dst] : ctx.Points)
    {
        (void)src;
        if (dst)
        {
            ApplyTransform(*dst, t);
        }
    }
    for (auto& [src, dst] : ctx.Curves)
    {
        (void)src;
        if (dst && !ApplyTransform(*dst, t))
        {
            return false;
        }
    }
    for (auto& [src, dst] : ctx.Surfaces)
    {
        (void)src;
        if (dst && !ApplyTransform(*dst, t))
        {
            return false;
        }
    }
    return true;
}

}  // namespace brep::boolean
