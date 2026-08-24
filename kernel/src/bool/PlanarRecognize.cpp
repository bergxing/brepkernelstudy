#include "brep/bool/PlanarRecognize.h"

#include "brep/bool/BoxRecognize.h"
#include "brep/Geometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace brep::boolean
{
namespace
{

[[nodiscard]] bool near_abs(double a, double b, double tol)
{
  return std::abs(a - b) <= tol;
}

[[nodiscard]] bool is_unit_axis(const Vector3d& n, double tol)
{
  const double ax = std::abs(n.x());
  const double ay = std::abs(n.y());
  const double az = std::abs(n.z());
  const double sum = ax + ay + az;
  if (!near_abs(sum, 1.0, tol * 10.0)) return false;
  return (ax > 0.5 && ay <= tol * 10.0 && az <= tol * 10.0) ||
         (ay > 0.5 && ax <= tol * 10.0 && az <= tol * 10.0) ||
         (az > 0.5 && ax <= tol * 10.0 && ay <= tol * 10.0);
}

[[nodiscard]] int axis_index(const Vector3d& n)
{
  const double ax = std::abs(n.x());
  const double ay = std::abs(n.y());
  const double az = std::abs(n.z());
  if (ax >= ay && ax >= az) return 0;
  if (ay >= ax && ay >= az) return 1;
  return 2;
}

[[nodiscard]] double ring_signed_area(const std::vector<Point2d>& ring)
{
  double a = 0.0;
  for (std::size_t i = 0; i < ring.size(); ++i)
  {
    const std::size_t j = (i + 1) % ring.size();
    a += ring[i].u() * ring[j].v() - ring[j].u() * ring[i].v();
  }
  return 0.5 * a;
}

void ensure_ccw(std::vector<Point2d>& ring)
{
  if (ring_signed_area(ring) < 0.0) std::reverse(ring.begin(), ring.end());
}

void ensure_cw(std::vector<Point2d>& ring)
{
  if (ring_signed_area(ring) > 0.0) std::reverse(ring.begin(), ring.end());
}

[[nodiscard]] std::vector<Point2d> loop_to_uv(const Loop& loop, const Plane& pl)
{
  std::vector<Point2d> uv;
  loop.ForEachCoedge([&](const CoEdge& ce)
  {
    if (Vertex* v = ce.From())
  {
      const Vector3d d = v->Position() - pl.Origin;
      uv.push_back(Point2d{d.dot(pl.UAxis), d.dot(pl.VAxis)});
    }
  });
  return uv;
}

}  // namespace

std::optional<PlanarPrismSpec> RecognizeExtrusionPrism(
    const Body& body, const BooleanContext& ctx)
{
  const double tol = std::max(ctx.fuzzy, 1e-12);
  if (RecognizeAxisAlignedBox(body, ctx)) return std::nullopt;
  if (body.Shells.size() != 1 || !body.Shells[0]) return std::nullopt;
  const Shell& shell = *body.Shells[0];
  if (!shell.Closed || shell.Faces.size() < 5) return std::nullopt;

  int axis_FaceCount[3] = {0, 0, 0};
  std::vector<const Face*> axis_faces[3];

  for (const Face* face : shell.Faces)
  {
    if (!face || !face->Surface) return std::nullopt;
    if (face->Surface->Kind() != SurfaceKind::Plane) return std::nullopt;
    const Vector3d n = face->NormalAt(0, 0);
    if (!is_unit_axis(n, tol)) continue;
    const int ai = axis_index(n);
    axis_faces[ai].push_back(face);
    ++axis_FaceCount[ai];
  }

  // Extrusion axis = unique axis with exactly two faces (the caps). Ortho
  // side faces inflate the other axes (e.g. L-prism).
  int axis = -1;
  for (int i = 0; i < 3; ++i)
  {
    if (axis_FaceCount[i] == 2)
  {
      if (axis >= 0) return std::nullopt;
      axis = i;
    }
  }
  if (axis < 0) return std::nullopt;

  const Face* f0 = axis_faces[axis][0];
  const Face* f1 = axis_faces[axis][1];
  const Vector3d n0 = f0->NormalAt(0, 0);
  const Vector3d n1 = f1->NormalAt(0, 0);
  // Caps must face opposite ways.
  if (n0.dot(n1) > -0.5) return std::nullopt;

  Vector3d axisDir{0, 0, 0};
  if (axis == 0) axisDir = Vector3d{1, 0, 0};
  else if (axis == 1) axisDir = Vector3d{0, 1, 0};
  else axisDir = Vector3d{0, 0, 1};

  // Bottom outward ≈ −axisDir, top outward ≈ +axisDir.
  const Face* bottom = (n0.dot(axisDir) < 0.0) ? f0 : f1;
  const Face* top = (bottom == f0) ? f1 : f0;
  if (top->NormalAt(0, 0).dot(axisDir) < 0.5) return std::nullopt;

  const Loop* bottom_outer = bottom->OuterLoop();
  if (!bottom_outer || bottom_outer->CoedgeCount() < 3) return std::nullopt;

  Point3d origin{};
  bool have_origin = false;
  bottom_outer->ForEachCoedge([&](const CoEdge& ce)
  {
    if (!have_origin)
  {
      if (Vertex* v = ce.From())
  {
        origin = v->Position();
        have_origin = true;
      }
    }
  });
  if (!have_origin) return std::nullopt;

  Vector3d uAxis{};
  bool have_u = false;
  bottom_outer->ForEachCoedge([&](const CoEdge& ce)
  {
    if (have_u) return;
    Vertex* a = ce.From();
    Vertex* b = ce.To();
    if (!a || !b) return;
    Vector3d e = b->Position() - a->Position();
    e = e - axisDir * e.dot(axisDir);
    if (e.norm() < tol * 10) return;
    uAxis = e.normalized();
    have_u = true;
  });
  if (!have_u) return std::nullopt;
  Vector3d vAxis = axisDir.cross(uAxis).normalized();
  if (uAxis.cross(vAxis).dot(axisDir) < 0) vAxis = -vAxis;

  Plane pl;
  pl.Origin = origin;
  pl.Normal = axisDir;
  pl.UAxis = uAxis;
  pl.VAxis = vAxis;

  const double h_bottom = (origin - pl.Origin).dot(axisDir);  // ~0
  (void)h_bottom;
  double d0 = 1e300;
  double d1 = -1e300;
  auto sample_height = [&](const Face* face)
  {
    if (const Loop* loop = face->OuterLoop())
  {
      loop->ForEachCoedge([&](const CoEdge& ce)
  {
        if (Vertex* v = ce.From())
  {
          const double h = (v->Position() - pl.Origin).dot(axisDir);
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
  spec.Plane = pl;
  spec.d0 = d0;
  spec.d1 = d1;
  spec.Tolerance = tol;
  spec.Name = body.Name.empty() ? "prism" : body.Name;
  spec.Outer = loop_to_uv(*bottom_outer, pl);
  if (spec.Outer.size() < 3) return std::nullopt;
  ensure_ccw(spec.Outer);

  for (const Loop* loop : bottom->Loops)
  {
    if (!loop || loop->Type != LoopType::Inner) continue;
    auto hole = loop_to_uv(*loop, pl);
    if (hole.size() < 3) continue;
    ensure_cw(hole);
    spec.Holes.push_back(std::move(hole));
  }

  // Side faces should be perpendicular to caps (optional sanity).
  for (const Face* face : shell.Faces)
  {
    if (face == bottom || face == top) continue;
    const Vector3d n = face->NormalAt(0, 0);
    if (std::abs(n.dot(axisDir)) > 0.1) return std::nullopt;
  }

  return spec;
}

}  // namespace brep::boolean
