#include "brep/builder.hpp"

#include "brep/log.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace brep {
namespace {

constexpr int vid(int ix, int iy, int iz) noexcept {
  return ix | (iy << 1) | (iz << 2);
}

Point3d corner(const BoxSpec& s, int ix, int iy, int iz) {
  return {
      ix ? s.max.x() : s.min.x(),
      iy ? s.max.y() : s.min.y(),
      iz ? s.max.z() : s.min.z(),
  };
}

struct EdgeDef {
  int a;
  int b;
  const char* name;
};

constexpr EdgeDef kEdges[12] = {
    {vid(0, 0, 0), vid(1, 0, 0), "e00"},
    {vid(1, 0, 0), vid(1, 1, 0), "e01"},
    {vid(1, 1, 0), vid(0, 1, 0), "e02"},
    {vid(0, 1, 0), vid(0, 0, 0), "e03"},
    {vid(0, 0, 1), vid(1, 0, 1), "e10"},
    {vid(1, 0, 1), vid(1, 1, 1), "e11"},
    {vid(1, 1, 1), vid(0, 1, 1), "e12"},
    {vid(0, 1, 1), vid(0, 0, 1), "e13"},
    {vid(0, 0, 0), vid(0, 0, 1), "ez0"},
    {vid(1, 0, 0), vid(1, 0, 1), "ez1"},
    {vid(1, 1, 0), vid(1, 1, 1), "ez2"},
    {vid(0, 1, 0), vid(0, 1, 1), "ez3"},
};

struct FaceBuild {
  const char* name;
  Point3d origin;
  Vector3d u_axis;
  Vector3d v_axis;
  std::array<int, 4> edge_idx;
  std::array<bool, 4> forward;
  std::array<Point2d, 5> uv;
};

}  // namespace

Body* make_box(Model& model, const BoxSpec& spec) {
  if (!(spec.max.x() > spec.min.x() && spec.max.y() > spec.min.y() &&
        spec.max.z() > spec.min.z())) {
    BREP_ERROR("make_box: invalid extents min={} max={}", spec.min, spec.max);
    throw std::invalid_argument("make_box: max must be strictly greater than min");
  }

  BREP_INFO("make_box '{}' min={} max={} tol={:.3g}", spec.name, spec.min,
            spec.max, spec.tolerance);

  const double dx = spec.max.x() - spec.min.x();
  const double dy = spec.max.y() - spec.min.y();
  const double dz = spec.max.z() - spec.min.z();
  const double tol = spec.tolerance;

  std::array<Vertex*, 8> V{};
  for (int iz = 0; iz < 2; ++iz) {
    for (int iy = 0; iy < 2; ++iy) {
      for (int ix = 0; ix < 2; ++ix) {
        const int i = vid(ix, iy, iz);
        Point* p = model.make_point(corner(spec, ix, iy, iz), "p" + std::to_string(i));
        V[i] = model.make_vertex(p, tol, "v" + std::to_string(i));
      }
    }
  }

  std::array<Edge*, 12> E{};
  for (int i = 0; i < 12; ++i) {
    const EdgeDef& d = kEdges[i];
    LineCurve* curve = model.make_line(V[d.a]->position(), V[d.b]->position());
    E[i] = model.make_edge(curve, V[d.a], V[d.b], 0.0, curve->length(), tol, d.name);
  }

  const FaceBuild faces[6] = {
      // Loop must be CCW when viewed against the outward normal (-Z).
      {"f_zmin",
       {spec.min.x(), spec.min.y(), spec.min.z()},
       {1, 0, 0},
       {0, -1, 0},
       {3, 2, 1, 0},
       {false, false, false, false},
       {Point2d{0, 0}, Point2d{0, -dy}, Point2d{dx, -dy}, Point2d{dx, 0},
        Point2d{0, 0}}},
      {"f_zmax",
       {spec.min.x(), spec.min.y(), spec.max.z()},
       {1, 0, 0},
       {0, 1, 0},
       {4, 5, 6, 7},
       {true, true, true, true},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dy}, Point2d{0, dy},
        Point2d{0, 0}}},
      {"f_ymin",
       {spec.min.x(), spec.min.y(), spec.min.z()},
       {1, 0, 0},
       {0, 0, 1},
       {0, 9, 4, 8},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
      {"f_ymax",
       {spec.max.x(), spec.max.y(), spec.min.z()},
       {-1, 0, 0},
       {0, 0, 1},
       {2, 11, 6, 10},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
      {"f_xmin",
       {spec.min.x(), spec.min.y(), spec.min.z()},
       {0, 0, 1},
       {0, 1, 0},
       {8, 7, 11, 3},
       {true, false, false, true},
       {Point2d{0, 0}, Point2d{dz, 0}, Point2d{dz, dy}, Point2d{0, dy},
        Point2d{0, 0}}},
      {"f_xmax",
       {spec.max.x(), spec.min.y(), spec.min.z()},
       {0, 1, 0},
       {0, 0, 1},
       {1, 10, 5, 9},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dy, 0}, Point2d{dy, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
  };

  Body* body = model.make_body(BodyType::Solid, spec.name);
  Shell* shell = model.make_shell(true, spec.name + "_shell");
  body->shells.push_back(shell);

  std::array<std::vector<CoEdge*>, 12> by_edge{};

  for (const FaceBuild& fd : faces) {
    PlaneSurface* surf = model.make_plane(fd.origin, fd.u_axis, fd.v_axis, fd.name);
    Face* face = model.make_face(surf, Orientation::Forward, fd.name);
    shell->faces.push_back(face);
    Loop* loop = model.make_loop(face, LoopType::Outer, std::string(fd.name) + "_outer");

    std::array<CoEdge*, 4> ces{};
    for (int k = 0; k < 4; ++k) {
      const int ei = fd.edge_idx[k];
      const Orientation sense =
          fd.forward[k] ? Orientation::Forward : Orientation::Reversed;
      Curve2d* pc = model.make_line2d(fd.uv[k], fd.uv[k + 1]);
      ces[k] = model.make_coedge(
          E[ei], sense, pc, std::string(fd.name) + "_ce" + std::to_string(k));
      by_edge[static_cast<std::size_t>(ei)].push_back(ces[k]);
    }
    Model::link_loop(loop, ces);
  }

  for (int i = 0; i < 12; ++i) {
    if (by_edge[static_cast<std::size_t>(i)].size() != 2) {
      BREP_ERROR("make_box: edge[{}] radial degree={}", i,
                 by_edge[static_cast<std::size_t>(i)].size());
      throw std::runtime_error(
          "make_box: each edge must be shared by exactly two faces");
    }
    Model::pair_partners(by_edge[static_cast<std::size_t>(i)][0],
                         by_edge[static_cast<std::size_t>(i)][1]);
  }

  BREP_INFO("make_box '{}' done: 8 verts, 12 edges, 6 faces", spec.name);
  return body;
}

}  // namespace brep
