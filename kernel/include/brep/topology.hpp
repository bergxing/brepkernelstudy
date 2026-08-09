#pragma once

#include "brep/geometry.hpp"
#include "brep/iobject.hpp"
#include "brep/types.hpp"

#include <cstddef>
#include <vector>

namespace brep {

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

class Vertex : public Named {
 public:
  Point* point{nullptr};
  double tolerance{1e-7};
  std::vector<Edge*> edges;  // incident star

  [[nodiscard]] const Point3d& position() const;
};

class Edge : public Named {
 public:
  Curve* curve{nullptr};
  Vertex* v0{nullptr};
  Vertex* v1{nullptr};
  double t0{0.0};
  double t1{1.0};
  double tolerance{1e-7};
  std::vector<CoEdge*> radial;  // coedges around this edge

  [[nodiscard]] Vertex* start(Orientation sense) const noexcept {
    return sense == Orientation::Forward ? v0 : v1;
  }
  [[nodiscard]] Vertex* end(Orientation sense) const noexcept {
    return sense == Orientation::Forward ? v1 : v0;
  }
  [[nodiscard]] double param_at(Orientation sense, double local_t) const noexcept {
    // local_t in [0,1] along the coedge direction
    return sense == Orientation::Forward ? (t0 + (t1 - t0) * local_t)
                                         : (t1 + (t0 - t1) * local_t);
  }
};

/// Oriented use of an Edge inside a Loop (half-edge / coedge).
class CoEdge : public Named {
 public:
  Edge* edge{nullptr};
  Orientation sense{Orientation::Forward};
  CoEdge* next{nullptr};
  CoEdge* prev{nullptr};
  CoEdge* partner{nullptr};  // mate across the shared edge
  Curve2d* pcurve{nullptr};
  Loop* loop{nullptr};

  [[nodiscard]] Vertex* from() const noexcept {
    return edge ? edge->start(sense) : nullptr;
  }
  [[nodiscard]] Vertex* to() const noexcept {
    return edge ? edge->end(sense) : nullptr;
  }
  [[nodiscard]] Face* face() const noexcept;
};

class Loop : public Named {
 public:
  Face* face{nullptr};
  LoopType type{LoopType::Outer};
  CoEdge* first{nullptr};

  /// Visit coedges in cyclic order. Returns count.
  template <class Fn>
  std::size_t for_each_coedge(Fn&& fn) const {
    if (!first) return 0;
    std::size_t n = 0;
    CoEdge* c = first;
    do {
      fn(*c);
      c = c->next;
      ++n;
    } while (c && c != first);
    return n;
  }

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] bool is_closed() const;
};

class Face : public Named {
 public:
  Surface* surface{nullptr};
  Orientation sense{Orientation::Forward};  // face normal vs surface normal
  std::vector<Loop*> loops;
  double tolerance{1e-7};

  [[nodiscard]] Loop* outer_loop() const noexcept;
  [[nodiscard]] Vector3d normal_at(double u, double v) const;
};

class Shell : public Named {
 public:
  std::vector<Face*> faces;
  bool closed{false};

  [[nodiscard]] std::size_t face_count() const noexcept { return faces.size(); }
};

/// Topological root (solid/sheet/wire). IObject for document Guid identity;
/// `id` remains the session-local topology handle.
class Body final : public IObject {
 public:
  Id id{0};
  BodyType type{BodyType::Solid};
  std::vector<Shell*> shells;

  [[nodiscard]] ObjectKind kind() const noexcept override {
    return ObjectKind::Body;
  }

  [[nodiscard]] Shell* outer_shell() const noexcept {
    return shells.empty() ? nullptr : shells.front();
  }
};

// Helpers that need complete types
inline Face* CoEdge::face() const noexcept {
  return loop ? loop->face : nullptr;
}

}  // namespace brep
