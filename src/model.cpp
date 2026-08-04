#include "brep/model.hpp"

#include <stdexcept>
#include <utility>

namespace brep {

Point* Model::make_point(Vec3 xyz, std::string name) {
  Point* p = emplace(points_, xyz);
  p->name = std::move(name);
  return p;
}

LineCurve* Model::make_line(Vec3 a, Vec3 b, std::string /*name*/) {
  const Vec3 d = b - a;
  const double len = d.norm();
  auto curve = std::make_unique<LineCurve>(a, len > 0 ? d : Vec3{1, 0, 0});
  curve->set_length(len > 0 ? len : 1.0);
  curve->id = next_id();
  LineCurve* raw = curve.get();
  curves_.push_back(std::move(curve));
  return raw;
}

CircleCurve* Model::make_circle(Vec3 center, Vec3 normal, double radius,
                                std::string /*name*/) {
  auto curve = std::make_unique<CircleCurve>(center, normal, radius);
  curve->id = next_id();
  CircleCurve* raw = curve.get();
  curves_.push_back(std::move(curve));
  return raw;
}

LineCurve2d* Model::make_line2d(Vec2 a, Vec2 b) {
  auto c = std::make_unique<LineCurve2d>(a, b);
  c->id = next_id();
  LineCurve2d* raw = c.get();
  curves2d_.push_back(std::move(c));
  return raw;
}

PlaneSurface* Model::make_plane(Vec3 origin, Vec3 normal, std::string /*name*/) {
  auto s = std::make_unique<PlaneSurface>(origin, normal);
  s->id = next_id();
  PlaneSurface* raw = s.get();
  surfaces_.push_back(std::move(s));
  return raw;
}

PlaneSurface* Model::make_plane(Vec3 origin, Vec3 u_axis, Vec3 v_axis,
                                std::string /*name*/) {
  auto s = std::make_unique<PlaneSurface>(origin, u_axis, v_axis);
  s->id = next_id();
  PlaneSurface* raw = s.get();
  surfaces_.push_back(std::move(s));
  return raw;
}

Vertex* Model::make_vertex(Point* p, double tol, std::string name) {
  Vertex* v = emplace(vertices_);
  v->point = p;
  v->tolerance = tol;
  v->name = std::move(name);
  return v;
}

Edge* Model::make_edge(Curve* c, Vertex* v0, Vertex* v1, double t0, double t1,
                       double tol, std::string name) {
  Edge* e = emplace(edges_);
  e->curve = c;
  e->v0 = v0;
  e->v1 = v1;
  e->t0 = t0;
  e->t1 = t1;
  e->tolerance = tol;
  e->name = std::move(name);
  attach_edge_to_vertices(e);
  return e;
}

CoEdge* Model::make_coedge(Edge* e, Orientation sense, Curve2d* pcurve,
                           std::string name) {
  CoEdge* c = emplace(coedges_);
  c->edge = e;
  c->sense = sense;
  c->pcurve = pcurve;
  c->name = std::move(name);
  if (e) {
    e->radial.push_back(c);
  }
  return c;
}

Loop* Model::make_loop(Face* face, LoopType type, std::string name) {
  Loop* l = emplace(loops_);
  l->face = face;
  l->type = type;
  l->name = std::move(name);
  if (face) {
    face->loops.push_back(l);
  }
  return l;
}

Face* Model::make_face(Surface* s, Orientation sense, std::string name) {
  Face* f = emplace(faces_);
  f->surface = s;
  f->sense = sense;
  f->name = std::move(name);
  return f;
}

Shell* Model::make_shell(bool closed, std::string name) {
  Shell* sh = emplace(shells_);
  sh->closed = closed;
  sh->name = std::move(name);
  return sh;
}

Body* Model::make_body(BodyType type, std::string name) {
  Body* b = emplace(bodies_);
  b->type = type;
  b->name = std::move(name);
  return b;
}

void Model::link_loop(Loop* loop, std::span<CoEdge* const> coedges) {
  if (!loop) {
    throw std::invalid_argument("link_loop: null loop");
  }
  if (coedges.empty()) {
    throw std::invalid_argument("link_loop: empty coedge list");
  }

  const std::size_t n = coedges.size();
  for (std::size_t i = 0; i < n; ++i) {
    CoEdge* cur = coedges[i];
    if (!cur) {
      throw std::invalid_argument("link_loop: null coedge");
    }
    cur->loop = loop;
    cur->next = coedges[(i + 1) % n];
    cur->prev = coedges[(i + n - 1) % n];
  }
  loop->first = coedges.front();
}

void Model::pair_partners(CoEdge* a, CoEdge* b) {
  if (!a || !b) {
    throw std::invalid_argument("pair_partners: null coedge");
  }
  if (a->edge != b->edge) {
    throw std::invalid_argument("pair_partners: coedges must share the same Edge");
  }
  a->partner = b;
  b->partner = a;
}

void Model::attach_edge_to_vertices(Edge* e) {
  if (!e) {
    return;
  }
  if (e->v0) {
    e->v0->edges.push_back(e);
  }
  if (e->v1 && e->v1 != e->v0) {
    e->v1->edges.push_back(e);
  }
}

}  // namespace brep
