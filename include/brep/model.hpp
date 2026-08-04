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
  Point* make_point(Vec3 xyz, std::string name = {});
  LineCurve* make_line(Vec3 a, Vec3 b, std::string name = {});
  CircleCurve* make_circle(Vec3 center, Vec3 normal, double radius,
                           std::string name = {});
  LineCurve2d* make_line2d(Vec2 a, Vec2 b);
  PlaneSurface* make_plane(Vec3 origin, Vec3 normal, std::string name = {});
  PlaneSurface* make_plane(Vec3 origin, Vec3 u_axis, Vec3 v_axis,
                           std::string name = {});

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
