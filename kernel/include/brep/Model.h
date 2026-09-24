#pragma once

#include "brep/Geometry.h"
#include "brep/Topology.h"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace brep
{

struct ModelPoolStats
{
    std::size_t Points{0};
    std::size_t Curves{0};
    std::size_t Curves2d{0};
    std::size_t Surfaces{0};
    std::size_t Vertices{0};
    std::size_t Edges{0};
    std::size_t Coedges{0};
    std::size_t Loops{0};
    std::size_t Faces{0};
    std::size_t Shells{0};
    std::size_t Bodies{0};
};

/// Owns all geometry and topology. Cross-entity pointers are non-owning and
/// remain valid for the lifetime of this Model.
class Model
{
 public:
  Model() = default;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&&) noexcept = default;
  Model& operator=(Model&&) noexcept = default;

  // --- geometry factories --------------------------------------------------
  Point* MakePoint(Point3d xyz, std::string name = {});
  LineCurve* MakeLine(Point3d a, Point3d b, std::string name = {});
  CircleCurve* MakeCircle(Point3d center, Vector3d normal, double radius,
                          std::string name = {});
  BezierCurve* MakeBezier(Point3d p0, Point3d p1, Point3d p2, Point3d p3,
                          std::string name = {});
  BezierCurve* MakeBezier(std::vector<Point3d> cvs, std::string name = {});
  BezierCurve* MakeBezier(std::vector<Point3d> cvs, std::vector<double> weights,
                          std::string name);
  NurbsCurve* MakeNurbs(std::vector<Point3d> cvs, std::vector<double> weights,
                        std::vector<double> knots, std::string name = {});
  LineCurve2d* MakeLine2d(Point2d a, Point2d b);
  PolylineCurve2d* MakePolyline2d(std::vector<Point2d> points);
  PlaneSurface* MakePlane(Point3d origin, Vector3d normal, std::string name = {});
  PlaneSurface* MakePlane(Point3d origin, Vector3d u_axis, Vector3d v_axis,
                          std::string name = {});
  SphereSurface* MakeSphereSurface(Point3d center, double radius,
                                   std::string name = {});
  CylinderSurface* MakeCylinderSurface(Point3d origin, Vector3d axis,
                                       double radius, std::string name = {});

  // --- topology factories --------------------------------------------------
  Vertex* MakeVertex(Point* p, double tol = 1e-7, std::string name = {});
  Edge* MakeEdge(Curve* c, Vertex* v0, Vertex* v1, double t0, double t1,
                 double tol = 1e-7, std::string name = {});
  CoEdge* MakeCoedge(Edge* e, Orientation sense, Curve2d* pcurve = nullptr,
                      std::string name = {});
  Loop* MakeLoop(Face* face, LoopType type, std::string name = {});
  Face* MakeFace(Surface* s, Orientation sense = Orientation::Forward,
                 std::string name = {});
  Shell* MakeShell(bool closed = false, std::string name = {});
  Body* MakeBody(BodyType type = BodyType::Solid, std::string name = {});

  // --- wiring helpers ------------------------------------------------------
  static void LinkLoop(Loop* loop, std::span<CoEdge* const> coedges);
  static void PairPartners(CoEdge* a, CoEdge* b);
  static void AttachEdgeToVertices(Edge* e);

  [[nodiscard]] const std::vector<std::unique_ptr<Body>>& Bodies() const noexcept
  {
    return m_bodies;
  }

  /// Remove a Body and purge topology/geometry no longer reachable from any
  /// remaining Body in this Model.
  bool RemoveBody(const Guid& guid);

  [[nodiscard]] ModelPoolStats PoolStats() const noexcept;

  [[nodiscard]] Id NextId() noexcept
  {
    return ++m_idCounter;
  }

 private:
  template <class T, class... Args>
  T* emplace(std::vector<std::unique_ptr<T>>& store, Args&&... args)
  {
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    obj->Id = NextId();
    T* raw = obj.get();
    store.push_back(std::move(obj));
    return raw;
  }

  void PurgeUnreferencedTopologyAndGeometry();
  void ScrubTopologyBackReferences();

  Id m_idCounter{0};

  std::vector<std::unique_ptr<Point>> m_points;
  std::vector<std::unique_ptr<Curve>> m_curves;
  std::vector<std::unique_ptr<Curve2d>> m_curves2d;
  std::vector<std::unique_ptr<Surface>> m_surfaces;

  std::vector<std::unique_ptr<Vertex>> m_vertices;
  std::vector<std::unique_ptr<Edge>> m_edges;
  std::vector<std::unique_ptr<CoEdge>> m_coedges;
  std::vector<std::unique_ptr<Loop>> m_loops;
  std::vector<std::unique_ptr<Face>> m_faces;
  std::vector<std::unique_ptr<Shell>> m_shells;
  std::vector<std::unique_ptr<Body>> m_bodies;
};

}  // namespace brep
