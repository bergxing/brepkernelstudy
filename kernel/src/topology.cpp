#include "brep/topology.hpp"

#include "brep/log.hpp"

#include <stdexcept>

namespace brep {

const Point3d& Vertex::position() const {
  if (!point) {
    BREP_ERROR("Vertex id={} has no Point geometry", id);
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

Vector3d Face::normal_at(double u, double v) const {
  if (!surface) {
    BREP_ERROR("Face '{}' has no Surface geometry", name);
    throw std::logic_error("Face has no Surface geometry");
  }
  Vector3d n = surface->normal(u, v);
  return sense == Orientation::Forward ? n : -n;
}

}  // namespace brep
