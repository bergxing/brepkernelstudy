#include "brep/bool/planar_recognize.hpp"

#include "brep/bool/box_recognize.hpp"
#include "brep/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace brep::boolean {
namespace {

[[nodiscard]] bool near_abs(double a, double b, double tol) {
  return std::abs(a - b) <= tol;
}

[[nodiscard]] bool is_unit_axis(const Vector3d& n, double tol) {
  const double ax = std::abs(n.x());
  const double ay = std::abs(n.y());
  const double az = std::abs(n.z());
  const double sum = ax + ay + az;
  if (!near_abs(sum, 1.0, tol * 10.0)) return false;
  return (ax > 0.5 && ay <= tol * 10.0 && az <= tol * 10.0) ||
         (ay > 0.5 && ax <= tol * 10.0 && az <= tol * 10.0) ||
         (az > 0.5 && ax <= tol * 10.0 && ay <= tol * 10.0);
}

[[nodiscard]] int axis_index(const Vector3d& n) {
  const double ax = std::abs(n.x());
  const double ay = std::abs(n.y());
  const double az = std::abs(n.z());
  if (ax >= ay && ax >= az) return 0;
  if (ay >= ax && ay >= az) return 1;
  return 2;
}

[[nodiscard]] double ring_signed_area(const std::vector<Point2d>& ring) {
  double a = 0.0;
  for (std::size_t i = 0; i < ring.size(); ++i) {
    const std::size_t j = (i + 1) % ring.size();
    a += ring[i].u() * ring[j].v() - ring[j].u() * ring[i].v();
  }
  return 0.5 * a;
}

void ensure_ccw(std::vector<Point2d>& ring) {
  if (ring_signed_area(ring) < 0.0) std::reverse(ring.begin(), ring.end());
}

void ensure_cw(std::vector<Point2d>& ring) {
  if (ring_signed_area(ring) > 0.0) std::reverse(ring.begin(), ring.end());
}

[[nodiscard]] std::vector<Point2d> loop_to_uv(const Loop& loop, const Plane& pl) {
  std::vector<Point2d> uv;
  loop.for_each_coedge([&](const CoEdge& ce) {
    if (Vertex* v = ce.from()) {
      const Vector3d d = v->position() - pl.origin;
      uv.push_back(Point2d{d.dot(pl.u_axis), d.dot(pl.v_axis)});
    }
  });
  return uv;
}

}  // namespace

std::optional<PlanarPrismSpec> recognize_extrusion_prism(
    const Body& body, const BooleanContext& ctx) {
  const double tol = std::max(ctx.fuzzy, 1e-12);
  if (recognize_axis_aligned_box(body, ctx)) return std::nullopt;
  if (body.shells.size() != 1 || !body.shells[0]) return std::nullopt;
  const Shell& shell = *body.shells[0];
  if (!shell.closed || shell.faces.size() < 5) return std::nullopt;

  int axis_face_count[3] = {0, 0, 0};
  std::vector<const Face*> axis_faces[3];

  for (const Face* face : shell.faces) {
    if (!face || !face->surface) return std::nullopt;
    if (face->surface->kind() != SurfaceKind::Plane) return std::nullopt;
    const Vector3d n = face->normal_at(0, 0);
    if (!is_unit_axis(n, tol)) continue;
    const int ai = axis_index(n);
    axis_faces[ai].push_back(face);
    ++axis_face_count[ai];
  }

  // Extrusion axis = unique axis with exactly two faces (the caps). Ortho
  // side faces inflate the other axes (e.g. L-prism).
  int axis = -1;
  for (int i = 0; i < 3; ++i) {
    if (axis_face_count[i] == 2) {
      if (axis >= 0) return std::nullopt;
      axis = i;
    }
  }
  if (axis < 0) return std::nullopt;

  const Face* f0 = axis_faces[axis][0];
  const Face* f1 = axis_faces[axis][1];
  const Vector3d n0 = f0->normal_at(0, 0);
  const Vector3d n1 = f1->normal_at(0, 0);
  // Caps must face opposite ways.
  if (n0.dot(n1) > -0.5) return std::nullopt;

  Vector3d axis_dir{0, 0, 0};
  if (axis == 0) axis_dir = Vector3d{1, 0, 0};
  else if (axis == 1) axis_dir = Vector3d{0, 1, 0};
  else axis_dir = Vector3d{0, 0, 1};

  // Bottom outward ≈ −axis_dir, top outward ≈ +axis_dir.
  const Face* bottom = (n0.dot(axis_dir) < 0.0) ? f0 : f1;
  const Face* top = (bottom == f0) ? f1 : f0;
  if (top->normal_at(0, 0).dot(axis_dir) < 0.5) return std::nullopt;

  const Loop* bottom_outer = bottom->outer_loop();
  if (!bottom_outer || bottom_outer->size() < 3) return std::nullopt;

  Point3d origin{};
  bool have_origin = false;
  bottom_outer->for_each_coedge([&](const CoEdge& ce) {
    if (!have_origin) {
      if (Vertex* v = ce.from()) {
        origin = v->position();
        have_origin = true;
      }
    }
  });
  if (!have_origin) return std::nullopt;

  Vector3d u_axis{};
  bool have_u = false;
  bottom_outer->for_each_coedge([&](const CoEdge& ce) {
    if (have_u) return;
    Vertex* a = ce.from();
    Vertex* b = ce.to();
    if (!a || !b) return;
    Vector3d e = b->position() - a->position();
    e = e - axis_dir * e.dot(axis_dir);
    if (e.norm() < tol * 10) return;
    u_axis = e.normalized();
    have_u = true;
  });
  if (!have_u) return std::nullopt;
  Vector3d v_axis = axis_dir.cross(u_axis).normalized();
  if (u_axis.cross(v_axis).dot(axis_dir) < 0) v_axis = -v_axis;

  Plane pl;
  pl.origin = origin;
  pl.normal = axis_dir;
  pl.u_axis = u_axis;
  pl.v_axis = v_axis;

  const double h_bottom = (origin - pl.origin).dot(axis_dir);  // ~0
  (void)h_bottom;
  double d0 = 1e300;
  double d1 = -1e300;
  auto sample_height = [&](const Face* face) {
    if (const Loop* loop = face->outer_loop()) {
      loop->for_each_coedge([&](const CoEdge& ce) {
        if (Vertex* v = ce.from()) {
          const double h = (v->position() - pl.origin).dot(axis_dir);
          d0 = std::min(d0, h);
          d1 = std::max(d1, h);
        }
      });
    }
  };
  sample_height(bottom);
  sample_height(top);
  if (!(d1 > d0 + tol)) return std::nullopt;

  PlanarPrismSpec spec;
  spec.plane = pl;
  spec.d0 = d0;
  spec.d1 = d1;
  spec.tolerance = tol;
  spec.name = body.name.empty() ? "prism" : body.name;
  spec.outer = loop_to_uv(*bottom_outer, pl);
  if (spec.outer.size() < 3) return std::nullopt;
  ensure_ccw(spec.outer);

  for (const Loop* loop : bottom->loops) {
    if (!loop || loop->type != LoopType::Inner) continue;
    auto hole = loop_to_uv(*loop, pl);
    if (hole.size() < 3) continue;
    ensure_cw(hole);
    spec.holes.push_back(std::move(hole));
  }

  // Side faces should be perpendicular to caps (optional sanity).
  for (const Face* face : shell.faces) {
    if (face == bottom || face == top) continue;
    const Vector3d n = face->normal_at(0, 0);
    if (std::abs(n.dot(axis_dir)) > 0.1) return std::nullopt;
  }

  return spec;
}

}  // namespace brep::boolean
