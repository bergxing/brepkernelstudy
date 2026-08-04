#include "brep/topology.hpp"

#include <stdexcept>

namespace brep {

const Vec3& Vertex::position() const {
  if (!point) {
    throw std::logic_error("Vertex has no Point geometry");
  }
  return point->xyz();
}

std::size_t Loop::size() const {
  std::size_t n = 0;
  for_each_coedge([&](const CoEdge&) { ++n; });
  return n;
}

bool Loop::is_closed() const {
  if (!first || !first->prev) return false;
  CoEdge* c = first;
  do {
    if (!c->next || c->next->prev != c) return false;
    c = c->next;
  } while (c != first);
  return true;
}

Loop* Face::outer_loop() const noexcept {
  for (Loop* l : loops) {
    if (l && l->type == LoopType::Outer) return l;
  }
  return loops.empty() ? nullptr : loops.front();
}

Vec3 Face::normal_at(double u, double v) const {
  if (!surface) {
    throw std::logic_error("Face has no Surface geometry");
  }
  Vec3 n = surface->normal(u, v);
  return sense == Orientation::Forward ? n : -n;
}

}  // namespace brep
