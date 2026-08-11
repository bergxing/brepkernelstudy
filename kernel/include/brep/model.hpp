#pragma once

#include "brep/geometry.hpp"
#include "brep/topology.hpp"

#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace brep {

/// Owns all geometry and topology. Cross-entity pointers are non-owning and
/// remain valid for the lifetime of this Model.
class Model {
 public:
  Model() = default;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&&) noexcept = default;
  Model& operator=(Model&&) noexcept = default;

  // --- geometry factories --------------------------------------------------
  Point* make_point(Point3d xyz, std::string name = {});
  LineCurve* make_line(Point3d a, Point3d b, std::string name = {});
  CircleCurve* make_circle(Point3d center, Vector3d normal, double radius,
                           std::string name = {});
  LineCurve2d* make_line2d(Point2d a, Point2d b);
  PlaneSurface* make_plane(Point3d origin, Vector3d normal, std::string name = {});
  PlaneSurface* make_plane(Point3d origin, Vector3d u_axis, Vector3d v_axis,
                           std::string name = {});
  SphereSurface* make_sphere_surface(Point3d center, double radius,
                                     std::string name = {});
  CylinderSurface* make_cylinder_surface(Point3d origin, Vector3d axis,
                                         double radius, std::string name = {});

  // --- topology factories --------------------------------------------------
  Vertex* make_vertex(Point* p, double tol = 1e-7, std::string name = {});
  Edge* make_edge(Curve* c, Vertex* v0, Vertex* v1, double t0, double t1,
                  double tol = 1e-7, std::string name = {});
  CoEdge* make_coedge(Edge* e, Orientation sense, Curve2d* pcurve = nullptr,
                      std::string name = {});
  Loop* make_loop(Face* face, LoopType type, std::string name = {});
  Face* make_face(Surface* s, Orientation sense = Orientation::Forward,
                  std::string name = {});
  Shell* make_shell(bool closed = false, std::string name = {});
  Body* make_body(BodyType type = BodyType::Solid, std::string name = {});

  // --- wiring helpers ------------------------------------------------------
  static void link_loop(Loop* loop, std::span<CoEdge* const> coedges);
  static void pair_partners(CoEdge* a, CoEdge* b);
  static void attach_edge_to_vertices(Edge* e);

  [[nodiscard]] const std::vector<std::unique_ptr<Body>>& bodies() const noexcept {
    return bodies_;
  }

  /// Remove a Body from the ownership pool (geometry/topology orphans remain).
  bool remove_body(const Guid& guid);

  [[nodiscard]] Id next_id() noexcept { return ++id_counter_; }

 private:
  template <class T, class... Args>
  T* emplace(std::vector<std::unique_ptr<T>>& store, Args&&... args) {
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    obj->id = next_id();
    T* raw = obj.get();
    store.push_back(std::move(obj));
    return raw;
  }

  Id id_counter_{0};

  std::vector<std::unique_ptr<Point>> points_;
  std::vector<std::unique_ptr<Curve>> curves_;
  std::vector<std::unique_ptr<Curve2d>> curves2d_;
  std::vector<std::unique_ptr<Surface>> surfaces_;

  std::vector<std::unique_ptr<Vertex>> vertices_;
  std::vector<std::unique_ptr<Edge>> edges_;
  std::vector<std::unique_ptr<CoEdge>> coedges_;
  std::vector<std::unique_ptr<Loop>> loops_;
  std::vector<std::unique_ptr<Face>> faces_;
  std::vector<std::unique_ptr<Shell>> shells_;
  std::vector<std::unique_ptr<Body>> bodies_;
};

}  // namespace brep
