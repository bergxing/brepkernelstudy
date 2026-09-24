#pragma once

#include "brep/bool/Pipeline.h"
#include "brep/Model.h"
#include "brep/Topology.h"

namespace brep::boolean
{

struct ImprintResult
{
  bool Ok{false};
  Body* OutputBody{nullptr};
  std::string Diagnostics;
};

struct SplitEdgeResult
{
  bool Ok{false};
  Edge* FirstHalf{nullptr};
  Edge* SecondHalf{nullptr};
  Vertex* SplitVertex{nullptr};
};

/// Split a line edge at curve parameter `t` (must lie strictly inside the edge domain).
[[nodiscard]] SplitEdgeResult SplitEdgeAt(Model& model, Edge& edge, double t,
                                          double eps);

/// Imprint a 3D segment onto a planar face: split boundary edges and insert the chord.
[[nodiscard]] bool ImprintSegmentOnPlanarFace(Model& model, Shell& shell, Face& face,
                                                const Point3d& start,
                                                const Point3d& end, double eps);

/// Split a spherical face by a complete analytic circle away from its seam and
/// poles. The original face becomes the complement and a cap face is appended.
[[nodiscard]] bool ImprintClosedCircleOnSphereFace(
    Model& model, Shell& shell, Face& face, const Point3d& center,
    const Vector3d& normal, double radius, double eps);

/// Add an inner circular loop to a planar face (plane-sphere intersection disk).
[[nodiscard]] bool ImprintClosedCircleOnPlanarFace(
    Model& model, Shell& shell, Face& face, const Point3d& center,
    const Vector3d& normal, double radius, double eps);

/// Apply topological imprint from IntersectionGraph segments onto working bodies.
[[nodiscard]] ImprintResult RunTopologicalImprint(PipelineState& state);

/// Imprint stage entry: general pipeline only (topological imprint or fragment passthrough).
[[nodiscard]] ImprintResult RunImprint(PipelineState& state);

}  // namespace brep::boolean
