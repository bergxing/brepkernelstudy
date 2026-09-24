#include "brep/Model.h"

#include "brep/Log.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace brep
{
namespace
{

struct ReachablePools
{
    std::unordered_set<const Point*> points;
    std::unordered_set<const Curve*> curves;
    std::unordered_set<const Curve2d*> curves2d;
    std::unordered_set<const Surface*> surfaces;
    std::unordered_set<const Vertex*> vertices;
    std::unordered_set<const Edge*> edges;
    std::unordered_set<const CoEdge*> coedges;
    std::unordered_set<const Loop*> loops;
    std::unordered_set<const Face*> faces;
    std::unordered_set<const Shell*> shells;
};

void MarkCoedge(CoEdge* coedge, ReachablePools& reachable);
void MarkEdge(Edge* edge, ReachablePools& reachable);
void MarkLoop(Loop* loop, ReachablePools& reachable);
void MarkFace(Face* face, ReachablePools& reachable);
void MarkShell(Shell* shell, ReachablePools& reachable);

void MarkCoedge(CoEdge* coedge, ReachablePools& reachable)
{
    if (!coedge || !reachable.coedges.insert(coedge).second)
    {
        return;
    }
    MarkEdge(coedge->Edge, reachable);
    if (coedge->Partner)
    {
        MarkCoedge(coedge->Partner, reachable);
    }
    if (coedge->Pcurve)
    {
        reachable.curves2d.insert(coedge->Pcurve);
    }
}

void MarkEdge(Edge* edge, ReachablePools& reachable)
{
    if (!edge || !reachable.edges.insert(edge).second)
    {
        return;
    }
    if (edge->Curve)
    {
        reachable.curves.insert(edge->Curve);
    }
    if (edge->V0)
    {
        reachable.vertices.insert(edge->V0);
        if (edge->V0->Point)
        {
            reachable.points.insert(edge->V0->Point);
        }
    }
    if (edge->V1)
    {
        reachable.vertices.insert(edge->V1);
        if (edge->V1->Point)
        {
            reachable.points.insert(edge->V1->Point);
        }
    }
    for (CoEdge* coedge : edge->Radial)
    {
        MarkCoedge(coedge, reachable);
    }
}

void MarkLoop(Loop* loop, ReachablePools& reachable)
{
    if (!loop || !reachable.loops.insert(loop).second)
    {
        return;
    }
    if (loop->Face)
    {
        MarkFace(loop->Face, reachable);
    }
    loop->ForEachCoedge(
        [&](const CoEdge& coedge)
        {
            MarkCoedge(const_cast<CoEdge*>(&coedge), reachable);
        });
}

void MarkFace(Face* face, ReachablePools& reachable)
{
    if (!face || !reachable.faces.insert(face).second)
    {
        return;
    }
    if (face->Surface)
    {
        reachable.surfaces.insert(face->Surface);
    }
    for (Loop* loop : face->Loops)
    {
        MarkLoop(loop, reachable);
    }
}

void MarkShell(Shell* shell, ReachablePools& reachable)
{
    if (!shell || !reachable.shells.insert(shell).second)
    {
        return;
    }
    for (Face* face : shell->Faces)
    {
        MarkFace(face, reachable);
    }
}

ReachablePools CollectReachable(const Model& model)
{
    ReachablePools reachable;
    for (const std::unique_ptr<Body>& body : model.Bodies())
    {
        for (Shell* shell : body->Shells)
        {
            MarkShell(shell, reachable);
        }
        for (Edge* edge : body->WireEdges)
        {
            MarkEdge(edge, reachable);
        }
    }
    return reachable;
}

template <typename T>
void PurgePool(std::vector<std::unique_ptr<T>>& pool,
               const std::unordered_set<const T*>& keep)
{
    pool.erase(
        std::remove_if(
            pool.begin(), pool.end(),
            [&](const std::unique_ptr<T>& object)
            {
                return keep.find(object.get()) == keep.end();
            }),
        pool.end());
}

}  // namespace

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

BezierCurve* Model::MakeBezier(Point3d p0, Point3d p1, Point3d p2, Point3d p3,
                               std::string /*name*/)
{
    auto curve = std::make_unique<BezierCurve>(p0, p1, p2, p3);
    curve->id = NextId();
    BezierCurve* raw = curve.get();
    m_curves.push_back(std::move(curve));
    BREP_TRACE("make_bezier id={} P0={} P3={}", raw->id, p0, p3);
    return raw;
}

BezierCurve* Model::MakeBezier(std::vector<Point3d> cvs, std::string /*name*/)
{
    auto curve = std::make_unique<BezierCurve>(std::move(cvs));
    curve->id = NextId();
    BezierCurve* raw = curve.get();
    m_curves.push_back(std::move(curve));
    BREP_TRACE("make_bezier id={} degree={}", raw->id, raw->Degree());
    return raw;
}

BezierCurve* Model::MakeBezier(std::vector<Point3d> cvs,
                               std::vector<double> weights, std::string /*name*/)
{
    auto curve =
        std::make_unique<BezierCurve>(std::move(cvs), std::move(weights));
    curve->id = NextId();
    BezierCurve* raw = curve.get();
    m_curves.push_back(std::move(curve));
    BREP_TRACE("make_bezier id={} degree={}", raw->id, raw->Degree());
    return raw;
}

NurbsCurve* Model::MakeNurbs(std::vector<Point3d> cvs,
                             std::vector<double> weights,
                             std::vector<double> knots, std::string /*name*/)
{
    auto curve = std::make_unique<NurbsCurve>(std::move(cvs), std::move(weights),
                                              std::move(knots));
    curve->id = NextId();
    NurbsCurve* raw = curve.get();
    m_curves.push_back(std::move(curve));
    BREP_TRACE("make_nurbs id={} degree={} cvs={}", raw->id, raw->Degree(),
               raw->Cvs().size());
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

PolylineCurve2d* Model::MakePolyline2d(std::vector<Point2d> points)
{
  auto curve = std::make_unique<PolylineCurve2d>(std::move(points));
  curve->id = NextId();
  PolylineCurve2d* raw = curve.get();
  m_curves2d.push_back(std::move(curve));
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
      PurgeUnreferencedTopologyAndGeometry();
      return true;
    }
  }
  return false;
}

ModelPoolStats Model::PoolStats() const noexcept
{
  return ModelPoolStats{
      .Points = m_points.size(),
      .Curves = m_curves.size(),
      .Curves2d = m_curves2d.size(),
      .Surfaces = m_surfaces.size(),
      .Vertices = m_vertices.size(),
      .Edges = m_edges.size(),
      .Coedges = m_coedges.size(),
      .Loops = m_loops.size(),
      .Faces = m_faces.size(),
      .Shells = m_shells.size(),
      .Bodies = m_bodies.size(),
  };
}

void Model::PurgeUnreferencedTopologyAndGeometry()
{
  const ReachablePools reachable = CollectReachable(*this);
  PurgePool(m_shells, reachable.shells);
  PurgePool(m_faces, reachable.faces);
  PurgePool(m_loops, reachable.loops);
  PurgePool(m_coedges, reachable.coedges);
  PurgePool(m_edges, reachable.edges);
  PurgePool(m_vertices, reachable.vertices);
  PurgePool(m_surfaces, reachable.surfaces);
  PurgePool(m_curves2d, reachable.curves2d);
  PurgePool(m_curves, reachable.curves);
  PurgePool(m_points, reachable.points);
  ScrubTopologyBackReferences();
}

void Model::ScrubTopologyBackReferences()
{
  const ReachablePools reachable = CollectReachable(*this);

  for (const std::unique_ptr<Vertex>& vertex : m_vertices)
  {
    auto& incident = vertex->Edges;
    incident.erase(
        std::remove_if(
            incident.begin(), incident.end(),
            [&](Edge* edge)
            {
                return reachable.edges.find(edge) == reachable.edges.end();
            }),
        incident.end());
  }

  for (const std::unique_ptr<Edge>& edge : m_edges)
  {
    auto& radial = edge->Radial;
    radial.erase(
        std::remove_if(
            radial.begin(), radial.end(),
            [&](CoEdge* coedge)
            {
                return reachable.coedges.find(coedge) == reachable.coedges.end();
            }),
        radial.end());
  }

  for (const std::unique_ptr<CoEdge>& coedge : m_coedges)
  {
    if (coedge->Partner &&
        reachable.coedges.find(coedge->Partner) == reachable.coedges.end())
    {
      coedge->Partner = nullptr;
    }
    if (coedge->Loop && reachable.loops.find(coedge->Loop) == reachable.loops.end())
    {
      coedge->Loop = nullptr;
    }
    if (coedge->Next &&
        reachable.coedges.find(coedge->Next) == reachable.coedges.end())
    {
      coedge->Next = nullptr;
    }
    if (coedge->Prev &&
        reachable.coedges.find(coedge->Prev) == reachable.coedges.end())
    {
      coedge->Prev = nullptr;
    }
  }

  for (const std::unique_ptr<Loop>& loop : m_loops)
  {
    if (loop->Face && reachable.faces.find(loop->Face) == reachable.faces.end())
    {
      loop->Face = nullptr;
    }
    if (loop->First &&
        reachable.coedges.find(loop->First) == reachable.coedges.end())
    {
      loop->First = nullptr;
    }
  }

  for (const std::unique_ptr<Face>& face : m_faces)
  {
    auto& loops = face->Loops;
    loops.erase(
        std::remove_if(
            loops.begin(), loops.end(),
            [&](Loop* loop)
            {
                return reachable.loops.find(loop) == reachable.loops.end();
            }),
        loops.end());
  }

  for (const std::unique_ptr<Shell>& shell : m_shells)
  {
    auto& faces = shell->Faces;
    faces.erase(
        std::remove_if(
            faces.begin(), faces.end(),
            [&](Face* face)
            {
                return reachable.faces.find(face) == reachable.faces.end();
            }),
        faces.end());
  }

  for (const std::unique_ptr<Body>& body : m_bodies)
  {
    auto& shells = body->Shells;
    shells.erase(
        std::remove_if(
            shells.begin(), shells.end(),
            [&](Shell* shell)
            {
                return reachable.shells.find(shell) == reachable.shells.end();
            }),
        shells.end());

    auto& wireEdges = body->WireEdges;
    wireEdges.erase(
        std::remove_if(
            wireEdges.begin(), wireEdges.end(),
            [&](Edge* edge)
            {
                return reachable.edges.find(edge) == reachable.edges.end();
            }),
        wireEdges.end());
  }
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
