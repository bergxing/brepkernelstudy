#include "brep/Model.h"

#include "brep/Log.h"

#include <stdexcept>
#include <utility>

namespace brep
{

Point* Model::MakePoint(Point3d xyz, std::string name)
{
  Point* p = emplace(m_points, xyz);
  p->Name = std::move(name);
  BREP_TRACE("MakePoint id={} name='{}' xyz={}", p->Id, p->Name, xyz);
  return p;
}

LineCurve* Model::MakeLine(Point3d a, Point3d b, std::string /*name*/)
{
  const Vector3d d = b - a;
  const double len = d.norm();
  auto curve = std::make_unique<LineCurve>(a, len > 0 ? d : Vector3d{1, 0, 0});
  curve->SetLength(len > 0 ? len : 1.0);
  curve->id = NextId();
  LineCurve* raw = curve.get();
  m_curves.push_back(std::move(curve));
  BREP_TRACE("make_line id={} len={:.6g} {} -> {}", raw->id, raw->Length(), a, b);
  return raw;
}

CircleCurve* Model::MakeCircle(Point3d center, Vector3d normal, double radius,
                                std::string /*name*/)
                                {
  auto curve = std::make_unique<CircleCurve>(center, normal, radius);
  curve->id = NextId();
  CircleCurve* raw = curve.get();
  m_curves.push_back(std::move(curve));
  BREP_TRACE("make_circle id={} center={} r={:.6g}", raw->id, center, radius);
  return raw;
}

LineCurve2d* Model::MakeLine2d(Point2d a, Point2d b)
{
  auto c = std::make_unique<LineCurve2d>(a, b);
  c->id = NextId();
  LineCurve2d* raw = c.get();
  m_curves2d.push_back(std::move(c));
  return raw;
}

PlaneSurface* Model::MakePlane(Point3d origin, Vector3d normal,
                                std::string /*name*/)
                                {
  auto s = std::make_unique<PlaneSurface>(origin, normal);
  s->id = NextId();
  PlaneSurface* raw = s.get();
  m_surfaces.push_back(std::move(s));
  BREP_TRACE("make_plane id={} origin={} normal={}", raw->id, origin, normal);
  return raw;
}

PlaneSurface* Model::MakePlane(Point3d origin, Vector3d u_axis, Vector3d v_axis,
                                std::string /*name*/)
                                {
  auto s = std::make_unique<PlaneSurface>(origin, u_axis, v_axis);
  s->id = NextId();
  PlaneSurface* raw = s.get();
  m_surfaces.push_back(std::move(s));
  BREP_DEBUG("make_plane id={} origin={} u={} v={}", raw->id, origin, u_axis,
             v_axis);
  return raw;
}

SphereSurface* Model::MakeSphereSurface(Point3d center, double radius,
                                          std::string /*name*/)
                                          {
  auto s = std::make_unique<SphereSurface>(center, radius);
  s->id = NextId();
  SphereSurface* raw = s.get();
  m_surfaces.push_back(std::move(s));
  BREP_TRACE("MakeSphere_surface id={} center={} r={:.6g}", raw->id, center,
             radius);
  return raw;
}

CylinderSurface* Model::MakeCylinderSurface(Point3d origin, Vector3d axis,
                                              double radius,
                                              std::string /*name*/)
                                              {
  auto s = std::make_unique<CylinderSurface>(origin, axis, radius);
  s->id = NextId();
  CylinderSurface* raw = s.get();
  m_surfaces.push_back(std::move(s));
  BREP_TRACE("make_cylinder_surface id={} origin={} axis={} r={:.6g}", raw->id,
             origin, axis, radius);
  return raw;
}

Vertex* Model::MakeVertex(Point* p, double tol, std::string name)
{
  Vertex* v = emplace(m_vertices);
  v->Point = p;
  v->Tolerance = tol;
  v->Name = std::move(name);
  return v;
}

Edge* Model::MakeEdge(Curve* c, Vertex* v0, Vertex* v1, double t0, double t1,
                       double tol, std::string name)
{
  Edge* e = emplace(m_edges);
  e->Curve = c;
  e->V0 = v0;
  e->V1 = v1;
  e->T0 = t0;
  e->T1 = t1;
  e->Tolerance = tol;
  e->Name = std::move(name);
  AttachEdgeToVertices(e);
  BREP_TRACE("make_edge id={} name='{}' {} -- {}", e->Id, e->Name,
             v0 ? v0->Name : "?", v1 ? v1->Name : "?");
  return e;
}

CoEdge* Model::MakeCoedge(Edge* e, Orientation sense, Curve2d* pcurve,
                           std::string name)
{
  CoEdge* c = emplace(m_coedges);
  c->Edge = e;
  c->Sense = sense;
  c->Pcurve = pcurve;
  c->Name = std::move(name);
  if (e)
  {
    e->Radial.push_back(c);
  }
  return c;
}

Loop* Model::MakeLoop(Face* face, LoopType type, std::string name)
{
  Loop* l = emplace(m_loops);
  l->Face = face;
  l->Type = type;
  l->Name = std::move(name);
  if (face)
  {
    face->Loops.push_back(l);
  }
  return l;
}

Face* Model::MakeFace(Surface* s, Orientation sense, std::string name)
{
  Face* f = emplace(m_faces);
  f->Surface = s;
  f->Sense = sense;
  f->Name = std::move(name);
  BREP_DEBUG("make_face id={} name='{}'", f->Id, f->Name);
  return f;
}

Shell* Model::MakeShell(bool closed, std::string name)
{
  Shell* sh = emplace(m_shells);
  sh->Closed = closed;
  sh->Name = std::move(name);
  BREP_INFO("make_shell id={} name='{}' closed={}", sh->Id, sh->Name, closed);
  return sh;
}

Body* Model::MakeBody(BodyType type, std::string name)
{
  auto owned = std::make_unique<Body>();
  owned->id = NextId();
  owned->Type = type;
  owned->Name = std::move(name);
  Body* b = owned.get();
  m_bodies.push_back(std::move(owned));
  BREP_INFO("make_body id={} guid={} name='{}'", b->id, b->Guid.ToString(),
            b->Name);
  return b;
}

bool Model::RemoveBody(const Guid& guid)
{
  for (auto it = m_bodies.begin(); it != m_bodies.end(); ++it)
{
    if (*it && (*it)->Guid == guid)
{
      BREP_INFO("remove_body guid={} name='{}'", guid.ToString(), (*it)->Name);
      m_bodies.erase(it);
      return true;
    }
  }
  return false;
}

void Model::LinkLoop(Loop* loop, std::span<CoEdge* const> coedges)
{
  if (!loop)
{
    BREP_ERROR("link_loop: null loop");
    throw std::invalid_argument("link_loop: null loop");
  }
  if (coedges.empty())
  {
    BREP_ERROR("link_loop: empty coedge list on '{}'", loop->Name);
    throw std::invalid_argument("link_loop: empty coedge list");
  }

  const std::size_t n = coedges.size();
  for (std::size_t i = 0; i < n; ++i)
  {
    CoEdge* cur = coedges[i];
    if (!cur)
    {
      throw std::invalid_argument("link_loop: null coedge");
    }
    cur->Loop = loop;
    cur->Next = coedges[(i + 1) % n];
    cur->Prev = coedges[(i + n - 1) % n];
  }
  loop->First = coedges.front();
  BREP_TRACE("link_loop '{}' size={}", loop->Name, n);
}

void Model::PairPartners(CoEdge* a, CoEdge* b)
{
  if (!a || !b)
{
    throw std::invalid_argument("pair_partners: null coedge");
  }
  if (a->Edge != b->Edge)
  {
    BREP_ERROR("pair_partners: coedges do not share an edge ({} vs {})",
               a->Edge ? a->Edge->Name : "null",
               b->Edge ? b->Edge->Name : "null");
    throw std::invalid_argument("pair_partners: coedges must share the same Edge");
  }
  a->Partner = b;
  b->Partner = a;
}

void Model::AttachEdgeToVertices(Edge* e)
{
  if (!e)
{
    return;
  }
  if (e->V0)
  {
    e->V0->Edges.push_back(e);
  }
  if (e->V1 && e->V1 != e->V0)
  {
    e->V1->Edges.push_back(e);
  }
}

}  // namespace brep
