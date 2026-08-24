#pragma once

#include "brep/Geometry.h"
#include "brep/IObject.h"
#include "brep/Types.h"

#include <cstddef>
#include <vector>

namespace brep
{

class Vertex;
class Edge;
class CoEdge;
class Loop;
class Face;
class Shell;
class Body;

// ---------------------------------------------------------------------------
// Topology skeleton (cross-links are non-owning; Model owns entities)
// ---------------------------------------------------------------------------

class Vertex : public Named
{
 public:
  Point* Point{nullptr};
  double Tolerance{1e-7};
  std::vector<Edge*> Edges;  // incident star

  [[nodiscard]] const Point3d& Position() const;
};

class Edge : public Named
{
 public:
  Curve* Curve{nullptr};
  Vertex* V0{nullptr};
  Vertex* V1{nullptr};
  double T0{0.0};
  double T1{1.0};
  double Tolerance{1e-7};
  std::vector<CoEdge*> Radial;  // coedges around this edge

  [[nodiscard]] Vertex* Start(Orientation sense) const noexcept
  {
    return sense == Orientation::Forward ? V0 : V1;
  }
  [[nodiscard]] Vertex* End(Orientation sense) const noexcept
  {
    return sense == Orientation::Forward ? V1 : V0;
  }
  [[nodiscard]] double ParamAt(Orientation sense, double local_t) const noexcept
  {
    // local_t in [0,1] along the coedge direction
    return sense == Orientation::Forward ? (T0 + (T1 - T0) * local_t)
                                         : (T1 + (T0 - T1) * local_t);
  }
};

/// Oriented use of an Edge inside a Loop (half-edge / coedge).
class CoEdge : public Named
{
 public:
  Edge* Edge{nullptr};
  Orientation Sense{Orientation::Forward};
  CoEdge* Next{nullptr};
  CoEdge* Prev{nullptr};
  CoEdge* Partner{nullptr};  // mate across the shared edge
  Curve2d* Pcurve{nullptr};
  Loop* Loop{nullptr};

  [[nodiscard]] Vertex* From() const noexcept
  {
    return Edge ? Edge->Start(Sense) : nullptr;
  }
  [[nodiscard]] Vertex* To() const noexcept
  {
    return Edge ? Edge->End(Sense) : nullptr;
  }
  [[nodiscard]] Face* GetFace() const noexcept;
};

class Loop : public Named
{
 public:
  Face* Face{nullptr};
  LoopType Type{LoopType::Outer};
  CoEdge* First{nullptr};

  /// Visit coedges in cyclic order. Returns count.
  template <class Fn>
  std::size_t ForEachCoedge(Fn&& fn) const
  {
    if (!First) return 0;
    std::size_t n = 0;
    CoEdge* c = First;
    do {
      fn(*c);
      c = c->Next;
      ++n;
    } while (c && c != First);
    return n;
  }

  [[nodiscard]] std::size_t CoedgeCount() const;
  [[nodiscard]] bool IsClosed() const;
};

class Face : public Named
{
 public:
  Surface* Surface{nullptr};
  Orientation Sense{Orientation::Forward};  // face normal vs surface normal
  std::vector<Loop*> Loops;
  double Tolerance{1e-7};

  [[nodiscard]] Loop* OuterLoop() const noexcept;
  [[nodiscard]] std::vector<Loop*> OuterLoops() const;
  [[nodiscard]] std::vector<Loop*> InnerLoops() const;
  [[nodiscard]] Vector3d NormalAt(double u, double v) const;
};

class Shell : public Named
{
 public:
  std::vector<Face*> Faces;
  bool Closed{false};

  [[nodiscard]] std::size_t FaceCount() const noexcept
  {
    return Faces.size();
  }
};

/// Topological root (solid/sheet/wire). IObject for document Guid identity;
/// `id` remains the session-local topology handle.
class Body final : public IObject
{
 public:
  Id id{0};
  BodyType Type{BodyType::Solid};
  std::vector<Shell*> Shells;

  [[nodiscard]] ObjectKind Kind() const noexcept override
  {
    return ObjectKind::Body;
  }

  [[nodiscard]] Shell* OuterShell() const noexcept
  {
    return Shells.empty() ? nullptr : Shells.front();
  }
};

// Helpers that need complete types
inline Face* CoEdge::GetFace() const noexcept
{
  return Loop ? Loop->Face : nullptr;
}

}  // namespace brep
