#include "brep/model.hpp"

#include "brep/log.hpp"

#include <stdexcept>
#include <utility>

namespace brep {

Point* Model::make_point(Point3d xyz, std::string name) {
  Point* p = emplace(points_, xyz);
  p->name = std::move(name);
  BREP_TRACE("make_point id={} name='{}' xyz={}", p->id, p->name, xyz);
  return p;
}

LineCurve* Model::make_line(Point3d a, Point3d b, std::string /*name*/) {
  const Vector3d d = b - a;
  const double len = d.norm();
  auto curve = std::make_unique<LineCurve>(a, len > 0 ? d : Vector3d{1, 0, 0});
  curve->set_length(len > 0 ? len : 1.0);
  curve->id = next_id();
  LineCurve* raw = curve.get();
  curves_.push_back(std::move(curve));
  BREP_TRACE("make_line id={} len={:.6g} {} -> {}", raw->id, raw->length(), a, b);
  return raw;
}

CircleCurve* Model::make_circle(Point3d center, Vector3d normal, double radius,
                                std::string /*name*/) {
  auto curve = std::make_unique<CircleCurve>(center, normal, radius);
  curve->id = next_id();
  CircleCurve* raw = curve.get();
  curves_.push_back(std::move(curve));
  BREP_TRACE("make_circle id={} center={} r={:.6g}", raw->id, center, radius);
  return raw;
}

LineCurve2d* Model::make_line2d(Point2d a, Point2d b) {
  auto c = std::make_unique<LineCurve2d>(a, b);
  c->id = next_id();
  LineCurve2d* raw = c.get();
  curves2d_.push_back(std::move(c));
  return raw;
}

PlaneSurface* Model::make_plane(Point3d origin, Vector3d normal,
                                std::string /*name*/) {
  auto s = std::make_unique<PlaneSurface>(origin, normal);
  s->id = next_id();
  PlaneSurface* raw = s.get();
  surfaces_.push_back(std::move(s));
  BREP_TRACE("make_plane id={} origin={} normal={}", raw->id, origin, normal);
  return raw;
}

PlaneSurface* Model::make_plane(Point3d origin, Vector3d u_axis, Vector3d v_axis,
                                std::string /*name*/) {
  auto s = std::make_unique<PlaneSurface>(origin, u_axis, v_axis);
  s->id = next_id();
  PlaneSurface* raw = s.get();
  surfaces_.push_back(std::move(s));
  BREP_DEBUG("make_plane id={} origin={} u={} v={}", raw->id, origin, u_axis,
             v_axis);
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
  BREP_TRACE("make_edge id={} name='{}' {} -- {}", e->id, e->name,
             v0 ? v0->name : "?", v1 ? v1->name : "?");
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
  BREP_DEBUG("make_face id={} name='{}'", f->id, f->name);
  return f;
}

Shell* Model::make_shell(bool closed, std::string name) {
  Shell* sh = emplace(shells_);
  sh->closed = closed;
  sh->name = std::move(name);
  BREP_INFO("make_shell id={} name='{}' closed={}", sh->id, sh->name, closed);
  return sh;
}

Body* Model::make_body(BodyType type, std::string name) {
  auto owned = std::make_unique<Body>();
  owned->id = next_id();
  owned->type = type;
  owned->name = std::move(name);
  Body* b = owned.get();
  bodies_.push_back(std::move(owned));
  BREP_INFO("make_body id={} guid={} name='{}'", b->id, b->guid.to_string(),
            b->name);
  return b;
}

bool Model::remove_body(const Guid& guid) {
  for (auto it = bodies_.begin(); it != bodies_.end(); ++it) {
    if (*it && (*it)->guid == guid) {
      BREP_INFO("remove_body guid={} name='{}'", guid.to_string(), (*it)->name);
      bodies_.erase(it);
      return true;
    }
  }
  return false;
}

void Model::link_loop(Loop* loop, std::span<CoEdge* const> coedges) {
  if (!loop) {
    BREP_ERROR("link_loop: null loop");
    throw std::invalid_argument("link_loop: null loop");
  }
  if (coedges.empty()) {
    BREP_ERROR("link_loop: empty coedge list on '{}'", loop->name);
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
  BREP_TRACE("link_loop '{}' size={}", loop->name, n);
}

void Model::pair_partners(CoEdge* a, CoEdge* b) {
  if (!a || !b) {
    throw std::invalid_argument("pair_partners: null coedge");
  }
  if (a->edge != b->edge) {
    BREP_ERROR("pair_partners: coedges do not share an edge ({} vs {})",
               a->edge ? a->edge->name : "null",
               b->edge ? b->edge->name : "null");
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
