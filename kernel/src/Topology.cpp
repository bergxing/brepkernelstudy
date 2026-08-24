#include "brep/Topology.h"

#include "brep/Log.h"

#include <stdexcept>

namespace brep
{

const Point3d& Vertex::Position() const
{
  if (!Point)
  {
    BREP_ERROR("Vertex Id={} has no Point geometry", Id);
    throw std::logic_error("Vertex has no Point geometry");
  }
  return Point->Xyz();
}

std::size_t Loop::CoedgeCount() const
{
  std::size_t n = 0;
  ForEachCoedge([&](const CoEdge&)
  {
    ++n;
  });
  return n;
}

bool Loop::IsClosed() const
{
  if (!First || !First->Prev) return false;
  CoEdge* c = First;
  do {
    if (!c->Next || c->Next->Prev != c) return false;
    c = c->Next;
  } while (c != First);
  return true;
}

Loop* Face::OuterLoop() const noexcept
{
  for (Loop* l : Loops)
  {
    if (l && l->Type == LoopType::Outer) return l;
  }
  return Loops.empty() ? nullptr : Loops.front();
}

std::vector<Loop*> Face::OuterLoops() const
{
  std::vector<Loop*> outers;
  for (Loop* l : Loops)
  {
    if (l && l->Type == LoopType::Outer) outers.push_back(l);
  }
  return outers;
}

std::vector<Loop*> Face::InnerLoops() const
{
  std::vector<Loop*> inners;
  for (Loop* l : Loops)
  {
    if (l && l->Type == LoopType::Inner) inners.push_back(l);
  }
  return inners;
}

Vector3d Face::NormalAt(double u, double v) const
{
  if (!Surface)
  {
    BREP_ERROR("Face '{}' has no Surface geometry", Name);
    throw std::logic_error("Face has no Surface geometry");
  }
  Vector3d n = Surface->Normal(u, v);
  return Sense == Orientation::Forward ? n : -n;
}

}  // namespace brep
