#include "brep/Mesh.h"

#include "brep/Aspect.h"
#include "brep/Geometry.h"
#include "brep/Log.h"
#include "brep/internal/Polygon2d.h"
#include "brep/mesh/Cdt.h"
#include "brep/mesh/LoopSample.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace brep
{
namespace
{

using brep::internal::PointInPolygon2d;
using brep::internal::SignedArea2d;

struct EdgeKey
{
  Id a;
  Id b;
  bool operator==(const EdgeKey& o) const noexcept
  {
      return a == o.a && b == o.b; 
  }
};

struct EdgeKeyHash
{
  std::size_t operator()(const EdgeKey& k) const noexcept
  {
    return (static_cast<std::size_t>(k.a) * 1315423911u) ^
           static_cast<std::size_t>(k.b);
  }
};

EdgeKey make_edge_key(const Edge& e)
{
  const Id i0 = e.V0 ? e.V0->Id : 0;
  const Id i1 = e.V1 ? e.V1->Id : 0;
  return i0 < i1 ? EdgeKey{i0, i1} : EdgeKey{i1, i0};
}

/// Periodic seam: both radial coedges belong to the same face (e.g. sphere
/// meridian). Ordinary manifold edges have coedges on two different faces.
bool is_periodic_seam_edge(const Edge& e)
{
  if (e.Radial.size() != 2) return false;
  const CoEdge* a = e.Radial[0];
  const CoEdge* b = e.Radial[1];
  if (!a || !b || !a->Loop || !b->Loop) return false;
  return a->Loop->Face != nullptr && a->Loop->Face == b->Loop->Face;
}

int clamp_segments(int value, int lo, int hi)
{
  return std::clamp(value, std::min(lo, hi), std::max(lo, hi));
}

[[nodiscard]] double ring_signed_area2d(const std::vector<Point2d>& ring)
{
  return SignedArea2d(ring);
}

void ensure_ccw(std::vector<Point2d>& uv, std::vector<Point3d>& xyz)
{
  if (ring_signed_area2d(uv) < 0.0)
{
    std::reverse(uv.begin(), uv.end());
    std::reverse(xyz.begin(), xyz.end());
  }
}

void ensure_cw(std::vector<Point2d>& uv, std::vector<Point3d>& xyz)
{
  if (ring_signed_area2d(uv) > 0.0)
{
    std::reverse(uv.begin(), uv.end());
    std::reverse(xyz.begin(), xyz.end());
  }
}

[[nodiscard]] bool point_in_triangle3d(const Point3d& p, const Point3d& a,
                                       const Point3d& b, const Point3d& c,
                                       double eps = 1e-9)
                                       {
  const Vector3d n = (b - a).cross(c - a);
  if (n.squaredNorm() < 1e-24) return false;
  const Vector3d na = (b - a).cross(p - a);
  const Vector3d nb = (c - b).cross(p - b);
  const Vector3d nc = (a - c).cross(p - c);
  return na.dot(n) >= -eps && nb.dot(n) >= -eps && nc.dot(n) >= -eps;
}

void tessellate_plane_face(const Face& face, TriangleMesh& mesh,
                           const TessellationOptions& opts)
{
  const auto* plane = dynamic_cast<const PlaneSurface*>(face.Surface);
  if (!plane)
  {
    return;
  }

  for (const brep::mesh::FaceRegion& region :
       brep::mesh::GroupFaceRegions(face, *plane, opts))
  {
    std::vector<Point2d> outer_uv;
    std::vector<Point3d> outer_xyz;
    outer_uv.reserve(region.Outer.Points.size());
    outer_xyz.reserve(region.Outer.Points.size());
    for (const brep::mesh::SampledPoint& point : region.Outer.Points)
    {
      outer_uv.push_back(point.Uv);
      outer_xyz.push_back(point.Xyz);
    }
    if (outer_uv.size() < 3)
    {
      BREP_WARN("tessellate: face '{}' has <3 outer vertices", face.Name);
      continue;
    }
    ensure_ccw(outer_uv, outer_xyz);

    std::vector<std::vector<Point2d>> holes_uv;
    holes_uv.reserve(region.Holes.size());
    for (const brep::mesh::SampledRing& hole : region.Holes)
    {
      std::vector<Point2d> uv;
      std::vector<Point3d> xyz;
      uv.reserve(hole.Points.size());
      xyz.reserve(hole.Points.size());
      for (const brep::mesh::SampledPoint& point : hole.Points)
      {
        uv.push_back(point.Uv);
        xyz.push_back(point.Xyz);
      }
      if (uv.size() < 3)
      {
        continue;
      }
      ensure_cw(uv, xyz);
      holes_uv.push_back(std::move(uv));
    }

    const brep::mesh::CdtResult cdt =
        brep::mesh::triangulate_polygon_with_holes(outer_uv, holes_uv);
    if (!cdt.Ok)
    {
      BREP_WARN("tessellate: CDT failed for face '{}': {}", face.Name,
                cdt.Diagnostics);
      continue;
    }

    double u_min = std::numeric_limits<double>::infinity();
    double u_max = -std::numeric_limits<double>::infinity();
    double v_min = std::numeric_limits<double>::infinity();
    double v_max = -std::numeric_limits<double>::infinity();
    for (const brep::mesh::CdtVertex& vertex : cdt.Vertices)
    {
      u_min = std::min(u_min, vertex.Uv.u());
      u_max = std::max(u_max, vertex.Uv.u());
      v_min = std::min(v_min, vertex.Uv.v());
      v_max = std::max(v_max, vertex.Uv.v());
    }
    const double du = std::max(u_max - u_min, 1e-9);
    const double dv = std::max(v_max - v_min, 1e-9);
    const std::uint32_t base =
        static_cast<std::uint32_t>(mesh.Vertices.size());
    for (const brep::mesh::CdtVertex& vertex : cdt.Vertices)
    {
      MeshVertex mesh_vertex;
      mesh_vertex.Position = plane->Eval(vertex.Uv.u(), vertex.Uv.v());
      mesh_vertex.Normal = face.NormalAt(vertex.Uv.u(), vertex.Uv.v());
      mesh_vertex.Uv = Point2d{(vertex.Uv.u() - u_min) / du,
                               (vertex.Uv.v() - v_min) / dv};
      mesh.Vertices.push_back(mesh_vertex);
    }

    const bool flip_tris =
        plane->Normal(0.0, 0.0).dot(face.NormalAt(0.0, 0.0)) < 0.0;
    for (const brep::mesh::CdtTriangle& triangle : cdt.Triangles)
    {
      const auto a = base + static_cast<std::uint32_t>(triangle.v[0]);
      const auto b = base + static_cast<std::uint32_t>(triangle.v[1]);
      const auto c = base + static_cast<std::uint32_t>(triangle.v[2]);
      if (!flip_tris)
      {
        mesh.Indices.push_back(a);
        mesh.Indices.push_back(b);
        mesh.Indices.push_back(c);
      }
      else
      {
        mesh.Indices.push_back(a);
        mesh.Indices.push_back(c);
        mesh.Indices.push_back(b);
      }
    }
  }
}

std::pair<int, int> sphere_segment_counts(double radius,
                                          const TessellationOptions& opts)
{
  const double R = std::max(radius, 1e-12);
  const double h =
      opts.LinearDeflection > 0.0
          ? opts.LinearDeflection
          : std::max(0.02 * R, 1e-4);
  const double ang = std::max(opts.AngularDeflection, 1e-6);

  int nu = static_cast<int>(std::ceil(2.0 * std::numbers::pi / ang));

  // Great-circle chord height h = R (1 - cos(α/2)) ⇒ α = 2 acos(1 - h/R).
  const double ratio = std::clamp(1.0 - h / R, -1.0, 1.0);
  const double alpha = 2.0 * std::acos(ratio);
  int nv_from_linear =
      alpha > 1e-12
          ? static_cast<int>(std::ceil(std::numbers::pi / alpha))
          : opts.MaxVSegments;
  int nv_from_angular =
      static_cast<int>(std::ceil(std::numbers::pi / ang));
  int nv = std::max(nv_from_linear, nv_from_angular);

  nu = clamp_segments(nu, opts.MinUSegments, opts.MaxUSegments);
  nv = clamp_segments(nv, opts.MinVSegments, opts.MaxVSegments);
  return {nu, nv};
}

[[nodiscard]] double normalize_sphere_u(double u)
{
  constexpr double kTwoPi = 2.0 * std::numbers::pi;
  double u_mod = std::fmod(u, kTwoPi);
  if (u_mod < 0.0)
  {
    u_mod += kTwoPi;
  }
  if (u_mod >= kTwoPi)
  {
    u_mod = 0.0;
  }
  return u_mod;
}

[[nodiscard]] bool point_in_polygon_uv(const Point2d& point,
                                       const std::vector<Point2d>& ring)
{
  return PointInPolygon2d(point, ring);
}

[[nodiscard]] bool is_analytic_sphere_seam_outer(const Face& face)
{
  const Loop* loop = face.OuterLoop();
  if (!loop || loop->CoedgeCount() != 2)
  {
    return false;
  }
  const CoEdge* c0 = loop->First;
  const CoEdge* c1 = c0 ? c0->Next : nullptr;
  return c0 && c1 && c0->Edge != nullptr && c0->Edge == c1->Edge;
}

/// Three quarter-circle arcs: exterior ⅞-sphere patch (small loop bounds hole).
[[nodiscard]] bool is_trimmed_sphere_octant_outer(const Face& face)
{
  const Loop* loop = face.OuterLoop();
  if (!loop || loop->CoedgeCount() != 3)
  {
    return false;
  }
  bool all_circle = true;
  loop->ForEachCoedge([&](const CoEdge& ce)
  {
    if (!ce.Edge || !ce.Edge->Curve ||
        ce.Edge->Curve->Kind() != CurveKind::Circle)
    {
      all_circle = false;
    }
  });
  return all_circle;
}

[[nodiscard]] std::vector<Point3d> spherical_loop_corners(
    const Point3d& center, const std::vector<Point3d>& boundary)
{
  if (boundary.size() < 6)
{
    return boundary;
  }
  std::array<std::size_t, 3> corners{};
  std::array<double, 3> turns{-1.0, -1.0, -1.0};
  for (std::size_t i = 0; i < boundary.size(); ++i)
  {
    const std::size_t im = (i + boundary.size() - 1) % boundary.size();
    const std::size_t ip = (i + 1) % boundary.size();
    const Vector3d dm = (boundary[im] - center).normalized();
    const Vector3d dp = (boundary[ip] - center).normalized();
    const double turn = dm.cross(dp).norm();
    for (int slot = 0; slot < 3; ++slot)
    {
      if (turn > turns[static_cast<std::size_t>(slot)])
    {
        for (int j = 2; j > slot; --j)
    {
          turns[static_cast<std::size_t>(j)] =
              turns[static_cast<std::size_t>(j - 1)];
          corners[static_cast<std::size_t>(j)] =
              corners[static_cast<std::size_t>(j - 1)];
        }
        turns[static_cast<std::size_t>(slot)] = turn;
        corners[static_cast<std::size_t>(slot)] = i;
        break;
      }
    }
  }
  return {boundary[corners[0]], boundary[corners[1]], boundary[corners[2]]};
}

[[nodiscard]] Point3d spherical_polygon_interior_hint(
    const SphereSurface& sphere, const std::vector<Point3d>& boundary)
{
  const Point3d& center = sphere.Center();
  const double radius = sphere.Radius();
  if (boundary.size() < 3)
  {
    return boundary.empty() ? center : boundary.front();
  }

  const std::vector<Point3d> corners = spherical_loop_corners(center, boundary);
  if (corners.size() == 3)
  {
    Vector3d sum{0, 0, 0};
    for (const Point3d& p : corners)
    {
      sum += p - center;
    }
    if (sum.squaredNorm() > 1e-24)
    {
      return center + sum.normalized() * radius;
    }
  }

  Vector3d sum{0, 0, 0};
  for (const Point3d& p : boundary)
  {
    sum += p - center;
  }
  if (sum.squaredNorm() < 1e-24)
  {
    return boundary.front();
  }
  return center + sum.normalized() * radius;
}

/// Great-circle polygon test on the sphere. `interior_hint` must lie inside the
/// bounded spherical polygon described by `boundary`.
[[nodiscard]] bool point_in_spherical_polygon(
    const Point3d& p, const Point3d& center,
    const std::vector<Point3d>& boundary, const Point3d& interior_hint)
    {
  if (boundary.size() < 3)
    {
    return false;
  }
  const Vector3d pd = p - center;
  const Vector3d hint = interior_hint - center;
  if (pd.squaredNorm() < 1e-24 || hint.squaredNorm() < 1e-24)
  {
    return false;
  }
  const Vector3d pn = pd.normalized();
  const Vector3d hn = hint.normalized();
  for (std::size_t i = 0; i < boundary.size(); ++i)
  {
    const Vector3d a = boundary[i] - center;
    const Vector3d b = boundary[(i + 1) % boundary.size()] - center;
    const Vector3d n = a.cross(b);
    if (n.squaredNorm() < 1e-24)
    {
      continue;
    }
    const Vector3d nn = n.normalized();
    if (pn.dot(nn) * hn.dot(nn) < 0.0)
    {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool point_in_inward_spherical_octant(
    const Point3d& p, const Point3d& center, const Point3d& ax,
    const Point3d& ay, const Point3d& az, double eps = 1e-9)
    {
  const Vector3d vx = ax - center;
  const Vector3d vy = ay - center;
  const Vector3d vz = az - center;
  const Vector3d d = p - center;
  return d.dot(vx) >= -eps && d.dot(vy) >= -eps && d.dot(vz) >= -eps;
}

[[nodiscard]] bool point_on_spherical_face(
    const Point3d& p, const SphereSurface& sphere,
    const std::vector<Point3d>& boundary_xyz, bool uv_complement,
    bool useOctantFilter)
    {
  if (boundary_xyz.size() < 3)
    {
    return true;
  }
  if (uv_complement)
  {
    const std::vector<Point3d> corners =
        spherical_loop_corners(sphere.Center(), boundary_xyz);
    if (useOctantFilter && corners.size() == 3)
    {
      const bool in_octant = point_in_inward_spherical_octant(
          p, sphere.Center(), corners[0], corners[1], corners[2]);
      return !in_octant;
    }
  }
  const Point3d hint = spherical_polygon_interior_hint(sphere, boundary_xyz);
  const bool in_poly = point_in_spherical_polygon(
      p, sphere.Center(), boundary_xyz, hint);
  if (uv_complement)
  {
    return !in_poly;
  }
  const std::vector<Point3d> corners =
      spherical_loop_corners(sphere.Center(), boundary_xyz);
  if (useOctantFilter && corners.size() == 3)
  {
    return in_poly && point_in_inward_spherical_octant(
                          p, sphere.Center(), corners[0], corners[1],
                          corners[2]);
  }
  return in_poly;
}

[[nodiscard]] bool point_in_deleted_trim_patch(
    const Point3d& p, const SphereSurface& sphere,
    const std::vector<Point3d>& boundary, bool complement,
    bool useOctantFilter)
{
  if (boundary.size() < 3)
  {
    return false;
  }
  const Point3d hint = spherical_polygon_interior_hint(sphere, boundary);
  const bool in_poly = point_in_spherical_polygon(
      p, sphere.Center(), boundary, hint);
  if (!complement)
  {
    if (useOctantFilter)
    {
      const std::vector<Point3d> corners =
          spherical_loop_corners(sphere.Center(), boundary);
      if (corners.size() == 3)
      {
        const bool in_oct = point_in_inward_spherical_octant(
            p, sphere.Center(), corners[0], corners[1], corners[2]);
        return !(in_poly && in_oct);
      }
    }
    return !in_poly;
  }
  if (useOctantFilter)
  {
    const std::vector<Point3d> corners =
        spherical_loop_corners(sphere.Center(), boundary);
    if (corners.size() == 3)
    {
      const bool in_oct = point_in_inward_spherical_octant(
          p, sphere.Center(), corners[0], corners[1], corners[2]);
      return in_poly && in_oct;
    }
  }
  return in_poly;
}

/// True for exterior ⅞ spherical patches (Intersect builds the inward ⅛ patch).
[[nodiscard]] bool trimmed_sphere_is_complement_patch(const Face& face)
{
  if (face.Name.find("_sphere_imprint_patch") != std::string::npos ||
      face.Name.find("_circle_imprint_cap") != std::string::npos ||
      face.Name.find("_circle_imprint_disk") != std::string::npos)
  {
    return false;
  }
  return face.Name.find("Intersect") == std::string::npos;
}

[[nodiscard]] bool is_sphere_imprint_patch(const Face& face)
{
  return face.Name.find("_sphere_imprint") != std::string::npos ||
         face.Name.find("_imprint_patch") != std::string::npos ||
         face.Name.find("_circle_imprint_cap") != std::string::npos;
}

[[nodiscard]] Point3d project_to_sphere_surface(const Point3d& p,
                                                const Point3d& center,
                                                double radius)
{
  const Vector3d radial = p - center;
  if (radial.squaredNorm() < 1e-24)
  {
    return p;
  }
  const Vector3d unit = radial.normalized();
  return Point3d{center.x() + unit.x() * radius, center.y() + unit.y() * radius,
                 center.z() + unit.z() * radius};
}

[[nodiscard]] Point3d SlerpOnSphere(const Point3d& center, double radius,
                                    const Point3d& from, const Point3d& to,
                                    double t)
{
  Vector3d dirFrom = from - center;
  Vector3d dirTo = to - center;
  if (dirFrom.norm() <= 1e-9 || dirTo.norm() <= 1e-9)
  {
    return from;
  }
  const Vector3d unitFrom = dirFrom.normalized();
  const Vector3d unitTo = dirTo.normalized();
  const double dot = std::clamp(unitFrom.dot(unitTo), -1.0, 1.0);
  const double omega = std::acos(dot);
  if (omega <= 1e-9)
  {
    return center + unitFrom * radius;
  }
  const double s0 = std::sin((1.0 - t) * omega) / std::sin(omega);
  const double s1 = std::sin(t * omega) / std::sin(omega);
  const Vector3d dir = (s0 * unitFrom + s1 * unitTo).normalized();
  return center + dir * radius;
}

[[nodiscard]] const CircleCurve* FirstCircleCurveOnFace(const Face& face)
{
  for (const Loop* loop : face.Loops)
  {
    if (loop == nullptr || loop->First == nullptr)
    {
      continue;
    }
    CoEdge* coedge = loop->First;
    std::size_t guard = 0;
    do
    {
      if (++guard > 1024U)
      {
        break;
      }
      if (coedge->Edge != nullptr && coedge->Edge->Curve != nullptr &&
          coedge->Edge->Curve->Kind() == CurveKind::Circle)
      {
        return static_cast<const CircleCurve*>(coedge->Edge->Curve);
      }
      coedge = coedge->Next;
    } while (coedge != nullptr && coedge != loop->First);
  }
  return nullptr;
}

[[nodiscard]] Point3d ImprintCapPole(const SphereSurface& sphere,
                                     const CircleCurve& circle)
{
  Vector3d axis = circle.Center() - sphere.Center();
  if (axis.norm() <= 1e-9)
  {
    axis = circle.Normal();
  }
  if (axis.norm() <= 1e-9)
  {
    return sphere.Eval(0.25, 0.25);
  }
  return sphere.Center() + axis.normalized() * sphere.Radius();
}

void tessellate_sphere_cap_rings(const Face& face,
                                 const SphereSurface& sphere,
                                 const Point3d& pole,
                                 const std::vector<Point3d>& boundary,
                                 TriangleMesh& mesh, int ringCount)
{
  if (boundary.size() < 3U || ringCount < 1)
  {
    return;
  }

  const Point3d& center = sphere.Center();
  const double radius = sphere.Radius();
  const std::size_t sideCount = boundary.size();

  auto add_vertex = [&](const Point3d& position) -> std::uint32_t
  {
    const Point2d uv = sphere.ParamOf(position);
    MeshVertex vertex;
    vertex.Position = position;
    vertex.Normal = face.NormalAt(uv.u(), uv.v());
    vertex.Uv = Point2d{uv.u() / (2.0 * std::numbers::pi),
                        (uv.v() + 0.5 * std::numbers::pi) /
                            std::numbers::pi};
    mesh.Vertices.push_back(vertex);
    return static_cast<std::uint32_t>(mesh.Vertices.size() - 1U);
  };

  auto emit_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
  {
    const Vector3d area =
        (mesh.Vertices[b].Position - mesh.Vertices[a].Position)
            .cross(mesh.Vertices[c].Position - mesh.Vertices[a].Position);
    if (area.squaredNorm() < 1e-24)
    {
      return;
    }
    if (area.dot(mesh.Vertices[a].Normal) < 0.0)
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(c);
      mesh.Indices.push_back(b);
    }
    else
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(b);
      mesh.Indices.push_back(c);
    }
  };

  const std::uint32_t poleIndex = add_vertex(pole);
  std::vector<std::vector<std::uint32_t>> rings(
      static_cast<std::size_t>(ringCount));
  for (int ring = 1; ring <= ringCount; ++ring)
  {
    const double t =
        static_cast<double>(ring) / static_cast<double>(ringCount);
    rings[static_cast<std::size_t>(ring - 1)].reserve(sideCount);
    for (std::size_t i = 0; i < sideCount; ++i)
    {
      const Point3d point =
          SlerpOnSphere(center, radius, pole, boundary[i], t);
      rings[static_cast<std::size_t>(ring - 1)].push_back(add_vertex(point));
    }
  }

  const std::vector<std::uint32_t>& innerRing = rings.front();
  for (std::size_t i = 0; i < sideCount; ++i)
  {
    emit_tri(poleIndex, innerRing[i],
             innerRing[(i + 1U) % sideCount]);
  }
  for (int ring = 1; ring < ringCount; ++ring)
  {
    const std::vector<std::uint32_t>& prev = rings[static_cast<std::size_t>(ring - 1)];
    const std::vector<std::uint32_t>& curr = rings[static_cast<std::size_t>(ring)];
    for (std::size_t i = 0; i < sideCount; ++i)
    {
      const std::size_t next = (i + 1U) % sideCount;
      emit_tri(prev[i], curr[i], curr[next]);
      emit_tri(prev[i], curr[next], prev[next]);
    }
  }
}

void tessellate_sphere_boundary_fan(const Face& face,
                                    const SphereSurface& sphere,
                                    const std::vector<Point3d>& boundary,
                                    TriangleMesh& mesh)
{
  if (boundary.size() < 3)
  {
    return;
  }
  Vector3d sum{0.0, 0.0, 0.0};
  for (const Point3d& point : boundary)
  {
    sum += point - sphere.Center();
  }
  if (sum.squaredNorm() < 1e-24)
  {
    return;
  }
  const Point3d centroid =
      project_to_sphere_surface(sphere.Center() + sum, sphere.Center(),
                                sphere.Radius());
  const Point2d centroidUv = sphere.ParamOf(centroid);
  const std::uint32_t centroidIndex =
      static_cast<std::uint32_t>(mesh.Vertices.size());
  MeshVertex centroidVertex;
  centroidVertex.Position = centroid;
  centroidVertex.Normal = face.NormalAt(centroidUv.u(), centroidUv.v());
  centroidVertex.Uv = Point2d{0.5, 0.5};
  mesh.Vertices.push_back(centroidVertex);

  const std::uint32_t ringBase =
      static_cast<std::uint32_t>(mesh.Vertices.size());
  for (const Point3d& point : boundary)
  {
    const Point3d onSphere =
        project_to_sphere_surface(point, sphere.Center(), sphere.Radius());
    const Point2d uv = sphere.ParamOf(onSphere);
    MeshVertex vertex;
    vertex.Position = onSphere;
    vertex.Normal = face.NormalAt(uv.u(), uv.v());
    vertex.Uv = Point2d{0.5 + 0.5 * (uv.u() - centroidUv.u()) / std::numbers::pi,
                        0.5 + (uv.v() - centroidUv.v()) / std::numbers::pi};
    mesh.Vertices.push_back(vertex);
  }

  const auto emit = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
  {
    const Vector3d area =
        (mesh.Vertices[b].Position - mesh.Vertices[a].Position)
            .cross(mesh.Vertices[c].Position - mesh.Vertices[a].Position);
    if (area.squaredNorm() < 1e-24)
    {
      return;
    }
    if (area.dot(mesh.Vertices[a].Normal) < 0.0)
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(c);
      mesh.Indices.push_back(b);
    }
    else
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(b);
      mesh.Indices.push_back(c);
    }
  };

  const std::size_t count = boundary.size();
  for (std::size_t i = 0; i < count; ++i)
  {
    emit(centroidIndex, ringBase + static_cast<std::uint32_t>(i),
         ringBase + static_cast<std::uint32_t>((i + 1) % count));
  }
}

/// Full-sphere lat-long mesh with spherical imprint holes punched out.
void tessellate_sphere_seam_with_holes(const Face& face,
                                       const SphereSurface& sphere,
                                       TriangleMesh& mesh, int nu, int nv,
                                       const TessellationOptions& opts)
{
  (void)opts;
  struct SphericalHole
  {
    Point3d planeOrigin;
    Vector3d planeNormal;
    double circleRadius;
  };
  std::vector<SphericalHole> holes;
  holes.reserve(face.Loops.size());
  for (const Loop* inner : face.InnerLoops())
  {
    if (inner == nullptr)
    {
      continue;
    }
    const CircleCurve* circle = nullptr;
    inner->ForEachCoedge([&](const CoEdge& coedge)
    {
      if (circle != nullptr || coedge.Edge == nullptr ||
          coedge.Edge->Curve == nullptr ||
          coedge.Edge->Curve->Kind() != CurveKind::Circle)
      {
        return;
      }
      circle = static_cast<const CircleCurve*>(coedge.Edge->Curve);
    });
    if (circle == nullptr)
    {
      continue;
    }
    Vector3d axis = circle->Center() - sphere.Center();
    if (axis.norm() <= 1e-9)
    {
      axis = circle->Normal();
    }
    if (axis.norm() <= 1e-9)
    {
      continue;
    }
    const Point3d hint = sphere.Center() + axis.normalized() * sphere.Radius();
    Vector3d normal = circle->Normal();
    if (normal.norm() <= 1e-9)
    {
      normal = axis;
    }
    normal = normal.normalized();
    if ((hint - circle->Center()).dot(normal) < 0.0)
    {
      normal = -normal;
    }
    const double planeOffset =
        (sphere.Center() - circle->Center()).dot(normal);
    const double circleRadius = std::sqrt(std::max(
        0.0, sphere.Radius() * sphere.Radius() - planeOffset * planeOffset));
    holes.push_back({circle->Center(), normal, circleRadius});
  }
  if (holes.empty())
  {
    return;
  }

  constexpr double kPi = std::numbers::pi;
  constexpr double kTwoPi = 2.0 * kPi;

  auto add_vertex = [&](double u, double v) -> std::uint32_t
  {
    const double u_eval = normalize_sphere_u(u);
    MeshVertex mesh_vertex;
    mesh_vertex.Position = sphere.Eval(u_eval, v);
    mesh_vertex.Normal = face.NormalAt(u_eval, v);
    mesh_vertex.Uv = Point2d{u_eval / kTwoPi, (v + 0.5 * kPi) / kPi};
    mesh.Vertices.push_back(mesh_vertex);
    return static_cast<std::uint32_t>(mesh.Vertices.size() - 1);
  };

  auto emit_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
  {
    const Vector3d area =
        (mesh.Vertices[b].Position - mesh.Vertices[a].Position)
            .cross(mesh.Vertices[c].Position - mesh.Vertices[a].Position);
    if (area.squaredNorm() < 1e-24)
    {
      return;
    }
    if (area.dot(mesh.Vertices[a].Normal) < 0.0)
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(c);
      mesh.Indices.push_back(b);
    }
    else
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(b);
      mesh.Indices.push_back(c);
    }
  };

  const double onPlane = std::max(1e-8, 1e-6 * std::abs(sphere.Radius()));
  const double weldTol = std::max(1e-6, 1e-5 * std::abs(sphere.Radius()));
  std::vector<std::uint32_t> welded;
  auto add_position = [&](const Point3d& position) -> std::uint32_t
  {
    for (const std::uint32_t index : welded)
    {
      if (mesh.Vertices[index].Position.distance_to(position) <= weldTol)
      {
        return index;
      }
    }
    const Point2d uv = sphere.ParamOf(position);
    const std::uint32_t index = add_vertex(uv.u(), uv.v());
    welded.push_back(index);
    return index;
  };
  auto side_of = [](const SphericalHole& hole, const Point3d& position)
  {
    return (position - hole.planeOrigin).dot(hole.planeNormal);
  };
  // Dropping a whole triangle whose centroid falls in the cap leaves a
  // jagged rim inside the intersection circle, so the two spheres no longer
  // meet. Clip to the circle plane and project the cut back onto the sphere.
  const auto keep_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
  {
    std::vector<std::uint32_t> polygon{a, b, c};
    for (const SphericalHole& hole : holes)
    {
      if (polygon.size() < 3U)
      {
        return;
      }
      std::vector<std::uint32_t> clipped;
      clipped.reserve(polygon.size() + 2U);
      for (std::size_t i = 0; i < polygon.size(); ++i)
      {
        const std::uint32_t current = polygon[i];
        const std::uint32_t next = polygon[(i + 1U) % polygon.size()];
        const Point3d& currentPos = mesh.Vertices[current].Position;
        const Point3d& nextPos = mesh.Vertices[next].Position;
        const double currentSide = side_of(hole, currentPos);
        const double nextSide = side_of(hole, nextPos);
        const bool currentOut = currentSide <= onPlane;
        const bool nextOut = nextSide <= onPlane;
        if (currentOut)
        {
          clipped.push_back(current);
        }
        if (currentOut != nextOut &&
            std::abs(currentSide - nextSide) > 1e-15)
        {
          const double t = currentSide / (currentSide - nextSide);
          const Point3d onChord{
              currentPos.x() + (nextPos.x() - currentPos.x()) * t,
              currentPos.y() + (nextPos.y() - currentPos.y()) * t,
              currentPos.z() + (nextPos.z() - currentPos.z()) * t};
          Vector3d inPlane = onChord - hole.planeOrigin;
          inPlane = inPlane - hole.planeNormal * inPlane.dot(hole.planeNormal);
          const Point3d onCircle =
              hole.circleRadius > 1e-9 && inPlane.norm() > 1e-9
                  ? hole.planeOrigin + inPlane.normalized() * hole.circleRadius
                  : project_to_sphere_surface(onChord, sphere.Center(),
                                              sphere.Radius());
          clipped.push_back(add_position(onCircle));
        }
      }
      polygon.swap(clipped);
    }
    if (polygon.size() < 3U)
    {
      return;
    }
    std::vector<std::uint32_t> refined;
    refined.reserve(polygon.size() * 2U);
    for (std::size_t i = 0; i < polygon.size(); ++i)
    {
      const std::uint32_t current = polygon[i];
      const std::uint32_t next = polygon[(i + 1U) % polygon.size()];
      refined.push_back(current);
      for (const SphericalHole& hole : holes)
      {
        const Point3d& currentPos = mesh.Vertices[current].Position;
        const Point3d& nextPos = mesh.Vertices[next].Position;
        if (std::abs(side_of(hole, currentPos)) > onPlane * 20.0 ||
            std::abs(side_of(hole, nextPos)) > onPlane * 20.0)
        {
          continue;
        }
        const Vector3d from = currentPos - hole.planeOrigin;
        const Vector3d to = nextPos - hole.planeOrigin;
        if (from.norm() <= 1e-9 || to.norm() <= 1e-9)
        {
          break;
        }
        const double angle = std::atan2(hole.planeNormal.dot(from.cross(to)),
                                         from.dot(to));
        const int steps = std::max(
            1, static_cast<int>(std::ceil(std::abs(angle) / (10.0 * kPi / 180.0))));
        for (int step = 1; step < steps; ++step)
        {
          const double t = static_cast<double>(step) / static_cast<double>(steps);
          const double cosine = std::cos(angle * t);
          const double sine = std::sin(angle * t);
          const Vector3d unitFrom = from.normalized();
          const Vector3d binormal =
              hole.planeNormal.cross(unitFrom).normalized();
          const Vector3d dir =
              (cosine * unitFrom + sine * binormal) * hole.circleRadius;
          refined.push_back(add_position(hole.planeOrigin + dir));
        }
        break;
      }
    }
    polygon.swap(refined);
    for (std::size_t i = 1; i + 1 < polygon.size(); ++i)
    {
      emit_tri(polygon[0], polygon[i], polygon[i + 1U]);
    }
  };

  const std::uint32_t south_idx = add_vertex(0.0, -0.5 * kPi);
  std::vector<std::vector<std::uint32_t>> rings(
      static_cast<std::size_t>(nv - 1));
  for (int iv = 1; iv < nv; ++iv)
  {
    const double v =
        -0.5 * kPi + (kPi * static_cast<double>(iv) / static_cast<double>(nv));
    rings[static_cast<std::size_t>(iv - 1)].resize(
        static_cast<std::size_t>(nu));
    for (int iu = 0; iu < nu; ++iu)
    {
      const double u =
          kTwoPi * static_cast<double>(iu) / static_cast<double>(nu);
      rings[static_cast<std::size_t>(iv - 1)][static_cast<std::size_t>(iu)] =
          add_vertex(u, v);
    }
  }
  const std::uint32_t north_idx = add_vertex(0.0, 0.5 * kPi);

  for (int iu = 0; iu < nu; ++iu)
  {
    const int iu1 = (iu + 1) % nu;
    keep_tri(south_idx, rings[0][static_cast<std::size_t>(iu1)],
             rings[0][static_cast<std::size_t>(iu)]);
  }

  for (int iv = 0; iv < nv - 2; ++iv)
  {
    for (int iu = 0; iu < nu; ++iu)
    {
      const int iu1 = (iu + 1) % nu;
      const std::uint32_t a =
          rings[static_cast<std::size_t>(iv)][static_cast<std::size_t>(iu)];
      const std::uint32_t b =
          rings[static_cast<std::size_t>(iv)][static_cast<std::size_t>(iu1)];
      const std::uint32_t c =
          rings[static_cast<std::size_t>(iv + 1)]
              [static_cast<std::size_t>(iu1)];
      const std::uint32_t d =
          rings[static_cast<std::size_t>(iv + 1)]
              [static_cast<std::size_t>(iu)];
      keep_tri(a, b, c);
      keep_tri(a, c, d);
    }
  }

  for (int iu = 0; iu < nu; ++iu)
  {
    const int iu1 = (iu + 1) % nu;
    keep_tri(north_idx,
             rings[static_cast<std::size_t>(nv - 2)]
                 [static_cast<std::size_t>(iu)],
             rings[static_cast<std::size_t>(nv - 2)]
                 [static_cast<std::size_t>(iu1)]);
  }
}

void tessellate_sphere_latlong_grid(
    const Face& face, const SphereSurface& sphere, TriangleMesh& mesh, int nu,
    int nv, const std::vector<Point3d>* trim_boundary = nullptr,
    bool trim_complement = false)
{
  constexpr double kPi = std::numbers::pi;
  constexpr double kTwoPi = 2.0 * kPi;
  if (nu < 3 || nv < 2)
  {
    return;
  }

  auto add_vertex = [&](double u, double v) -> std::uint32_t
  {
    const double u_eval = normalize_sphere_u(u);
    MeshVertex mesh_vertex;
    mesh_vertex.Position = sphere.Eval(u_eval, v);
    mesh_vertex.Normal = face.NormalAt(u_eval, v);
    mesh_vertex.Uv = Point2d{u_eval / kTwoPi, (v + 0.5 * kPi) / kPi};
    mesh.Vertices.push_back(mesh_vertex);
    return static_cast<std::uint32_t>(mesh.Vertices.size() - 1);
  };

  auto emit_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
  {
    const Vector3d area =
        (mesh.Vertices[b].Position - mesh.Vertices[a].Position)
            .cross(mesh.Vertices[c].Position - mesh.Vertices[a].Position);
    if (area.squaredNorm() < 1e-24)
    {
      return;
    }
    if (area.dot(mesh.Vertices[a].Normal) < 0.0)
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(c);
      mesh.Indices.push_back(b);
    }
    else
    {
      mesh.Indices.push_back(a);
      mesh.Indices.push_back(b);
      mesh.Indices.push_back(c);
    }
  };

  auto keep_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
  {
    if (trim_boundary && trim_boundary->size() >= 3)
    {
      const Point3d& pa = mesh.Vertices[a].Position;
      const Point3d& pb = mesh.Vertices[b].Position;
      const Point3d& pc = mesh.Vertices[c].Position;
      const auto on_kept = [&](const Point3d& p)
      {
        return !point_in_deleted_trim_patch(
            p, sphere, *trim_boundary, trim_complement,
            !is_sphere_imprint_patch(face));
      };
      if (!on_kept(pa) && !on_kept(pb) && !on_kept(pc))
      {
        return;
      }
      if (!trim_complement)
      {
        const Point3d centroid = project_to_sphere_surface(
            Point3d{(pa.x() + pb.x() + pc.x()) / 3.0,
                    (pa.y() + pb.y() + pc.y()) / 3.0,
                    (pa.z() + pb.z() + pc.z()) / 3.0},
            sphere.Center(), sphere.Radius());
        if (!on_kept(centroid))
        {
          return;
        }
      }
    }
    emit_tri(a, b, c);
  };

  const std::uint32_t south_idx = add_vertex(0.0, -0.5 * kPi);
  std::vector<std::vector<std::uint32_t>> rings(static_cast<std::size_t>(nv - 1));
  for (int iv = 1; iv < nv; ++iv)
  {
    const double v =
        -0.5 * kPi + (kPi * static_cast<double>(iv) / static_cast<double>(nv));
    rings[static_cast<std::size_t>(iv - 1)].resize(static_cast<std::size_t>(nu));
    for (int iu = 0; iu < nu; ++iu)
    {
      const double u =
          kTwoPi * static_cast<double>(iu) / static_cast<double>(nu);
      rings[static_cast<std::size_t>(iv - 1)][static_cast<std::size_t>(iu)] =
          add_vertex(u, v);
    }
  }
  const std::uint32_t north_idx = add_vertex(0.0, 0.5 * kPi);

  for (int iu = 0; iu < nu; ++iu)
  {
    const int iu1 = (iu + 1) % nu;
    keep_tri(south_idx, rings[0][static_cast<std::size_t>(iu1)],
             rings[0][static_cast<std::size_t>(iu)]);
  }

  for (int iv = 0; iv < nv - 2; ++iv)
  {
    for (int iu = 0; iu < nu; ++iu)
    {
      const int iu1 = (iu + 1) % nu;
      const std::uint32_t a =
          rings[static_cast<std::size_t>(iv)][static_cast<std::size_t>(iu)];
      const std::uint32_t b =
          rings[static_cast<std::size_t>(iv)][static_cast<std::size_t>(iu1)];
      const std::uint32_t c =
          rings[static_cast<std::size_t>(iv + 1)][static_cast<std::size_t>(iu1)];
      const std::uint32_t d =
          rings[static_cast<std::size_t>(iv + 1)][static_cast<std::size_t>(iu)];
      keep_tri(a, b, c);
      keep_tri(a, c, d);
    }
  }

  for (int iu = 0; iu < nu; ++iu)
  {
    const int iu1 = (iu + 1) % nu;
    keep_tri(north_idx,
             rings[static_cast<std::size_t>(nv - 2)][static_cast<std::size_t>(iu)],
             rings[static_cast<std::size_t>(nv - 2)][static_cast<std::size_t>(iu1)]);
  }
}

/// True when the oriented face interior lies outside the UV polygon image of
/// the outer loop (large spherical patches such as a ⅞-ball).
[[nodiscard]] bool sphere_face_needs_uv_complement(
    const SphereSurface& sphere, const Face& face,
    const std::vector<Point2d>& outer_uv,
    const std::vector<Point3d>& outer_xyz)
    {
  if (outer_uv.size() < 3 || outer_uv.size() != outer_xyz.size())
    {
    return false;
  }
  const Point3d& center = sphere.Center();
  const double radius = sphere.Radius();
  constexpr double kPi = std::numbers::pi;

  for (std::size_t i = 0; i < outer_xyz.size(); ++i)
  {
    const std::size_t j = (i + 1) % outer_xyz.size();
    const Point3d& a = outer_xyz[i];
    const Point3d& b = outer_xyz[j];
    Vector3d tangent = b - a;
    if (tangent.squaredNorm() < 1e-18)
    {
      continue;
    }
    tangent = tangent.normalized();

    const Point2d umid{(outer_uv[i].u() + outer_uv[j].u()) * 0.5,
                       (outer_uv[i].v() + outer_uv[j].v()) * 0.5};
    const double u_eval = normalize_sphere_u(umid.u());
    const double v_eval =
        std::clamp(umid.v(), -0.5 * kPi + 1e-6, 0.5 * kPi - 1e-6);
    const Point3d mid = sphere.Eval(u_eval, v_eval);
    const Vector3d normal = face.NormalAt(u_eval, v_eval);
    // Face interior lies to the left of the coedge when the head is along the
    // face normal: tangent × normal (right-handed).
    Vector3d into_face = tangent.cross(normal);
    if (into_face.squaredNorm() < 1e-18)
    {
      continue;
    }
    into_face = into_face.normalized();

    const Point3d probed_raw = mid + into_face * (0.02 * radius);
    const Vector3d dir = probed_raw - center;
    if (dir.squaredNorm() < 1e-18)
    {
      continue;
    }
    const Point3d on_sphere = center + dir.normalized() * radius;
    Point2d probe_uv = sphere.ParamOf(on_sphere);
    while (probe_uv.u() - umid.u() > kPi)
    {
      probe_uv.u() -= 2.0 * kPi;
    }
    while (probe_uv.u() - umid.u() < -kPi)
    {
      probe_uv.u() += 2.0 * kPi;
    }
    // Align v near mid (poles are noisy).
    if (std::abs(probe_uv.v() - umid.v()) > 0.5 * kPi)
    {
      continue;
    }
    return !point_in_polygon_uv(probe_uv, outer_uv);
  }
  return false;
}

[[nodiscard]] std::vector<Point2d> sphere_steiner_lattice(
    const std::vector<Point2d>& outer_ccw,
    const std::vector<std::vector<Point2d>>& holes_cw, int nu, int nv)
    {
  if (outer_ccw.empty() || nu < 2 || nv < 2)
    {
    return {};
  }
  double u_min = outer_ccw.front().u();
  double u_max = u_min;
  double v_min = outer_ccw.front().v();
  double v_max = v_min;
  for (const Point2d& p : outer_ccw)
  {
    u_min = std::min(u_min, p.u());
    u_max = std::max(u_max, p.u());
    v_min = std::min(v_min, p.v());
    v_max = std::max(v_max, p.v());
  }
  const double du = u_max - u_min;
  const double dv = v_max - v_min;
  if (du < 1e-12 || dv < 1e-12)
  {
    return {};
  }

  std::vector<Point2d> steiner;
  steiner.reserve(static_cast<std::size_t>(nu - 1) *
                  static_cast<std::size_t>(nv - 1));
  for (int iv = 1; iv < nv; ++iv)
  {
    const double v =
        v_min + (static_cast<double>(iv) / static_cast<double>(nv)) * dv;
    for (int iu = 1; iu < nu; ++iu)
    {
      const double u =
          u_min + (static_cast<double>(iu) / static_cast<double>(nu)) * du;
      const Point2d p{u, v};
      if (!point_in_polygon_uv(p, outer_ccw))
      {
        continue;
      }
      bool in_hole = false;
      for (const auto& hole : holes_cw)
      {
        if (point_in_polygon_uv(p, hole))
      {
          in_hole = true;
          break;
        }
      }
      if (!in_hole)
      {
        steiner.push_back(p);
      }
    }
  }
  return steiner;
}

void tessellate_sphere_face(const Face& face, TriangleMesh& mesh,
                            const TessellationOptions& opts)
{
  const auto* sphere = dynamic_cast<const SphereSurface*>(face.Surface);
  if (!sphere)
  {
    return;
  }
  if (!face.OuterLoop())
  {
    BREP_WARN("tessellate: sphere face '{}' has no outer loop", face.Name);
    return;
  }

  constexpr double kPi = std::numbers::pi;
  constexpr double kTwoPi = 2.0 * kPi;
  const auto [nu, nv] = sphere_segment_counts(sphere->Radius(), opts);

  TessellationOptions tessOpts = opts;
  if (tessOpts.LinearDeflection <= 0.0)
  {
    tessOpts.LinearDeflection =
        std::max(0.02 * std::abs(sphere->Radius()), 1e-4);
  }

  if (is_analytic_sphere_seam_outer(face))
  {
    const std::size_t indexBefore = mesh.Indices.size();
    if (face.InnerLoops().empty())
    {
      tessellate_sphere_latlong_grid(face, *sphere, mesh, nu, nv);
    }
    else
    {
      tessellate_sphere_seam_with_holes(face, *sphere, mesh, nu, nv, tessOpts);
    }
    if (mesh.Indices.size() > indexBefore)
    {
      return;
    }
  }

  if (is_sphere_imprint_patch(face))
  {
    for (brep::mesh::FaceRegion region :
         brep::mesh::GroupFaceRegions(face, *sphere, tessOpts))
    {
      std::vector<Point3d> boundary;
      boundary.reserve(region.Outer.Points.size());
      for (const brep::mesh::SampledPoint& point : region.Outer.Points)
      {
        boundary.push_back(point.Xyz);
      }
      if (boundary.size() >= 3U)
      {
        if (face.Name.find("_circle_imprint_cap") != std::string::npos)
        {
          const CircleCurve* circle = FirstCircleCurveOnFace(face);
          const Point3d pole =
              circle != nullptr ? ImprintCapPole(*sphere, *circle)
                                : project_to_sphere_surface(
                                      spherical_polygon_interior_hint(
                                          *sphere, boundary),
                                      sphere->Center(), sphere->Radius());
          tessellate_sphere_cap_rings(face, *sphere, pole, boundary, mesh,
                                      nv);
        }
        else
        {
          tessellate_sphere_boundary_fan(face, *sphere, boundary, mesh);
        }
      }
      break;
    }
    return;
  }

  const std::size_t indexBefore = mesh.Indices.size();
  std::vector<Point3d> fallbackBoundary;
  if (is_trimmed_sphere_octant_outer(face))
  {
    for (brep::mesh::FaceRegion region :
         brep::mesh::GroupFaceRegions(face, *sphere, opts))
    {
      std::vector<Point3d> boundary;
      boundary.reserve(region.Outer.Points.size());
      for (const brep::mesh::SampledPoint& point : region.Outer.Points)
      {
        boundary.push_back(point.Xyz);
      }
      if (boundary.size() >= 3)
      {
        fallbackBoundary = boundary;
        const bool trim_complement = trimmed_sphere_is_complement_patch(face);
        tessellate_sphere_latlong_grid(face, *sphere, mesh, nu, nv, &boundary,
                                       trim_complement);
      }
      break;
    }
    if (mesh.Indices.size() > indexBefore)
    {
      return;
    }
    if (fallbackBoundary.size() >= 3)
    {
      tessellate_sphere_boundary_fan(face, *sphere, fallbackBoundary, mesh);
      if (mesh.Indices.size() > indexBefore)
      {
        return;
      }
    }
  }

  for (brep::mesh::FaceRegion region :
       brep::mesh::GroupFaceRegions(face, *sphere, opts))
  {
    region.Outer = brep::mesh::UnwrapSphereRing(std::move(region.Outer));
    for (brep::mesh::SampledRing& hole : region.Holes)
    {
      hole = brep::mesh::UnwrapSphereRing(std::move(hole));
    }

    std::vector<Point2d> outer_uv;
    std::vector<Point3d> outer_xyz;
    outer_uv.reserve(region.Outer.Points.size());
    outer_xyz.reserve(region.Outer.Points.size());
    for (const brep::mesh::SampledPoint& point : region.Outer.Points)
    {
      outer_uv.push_back(point.Uv);
      outer_xyz.push_back(point.Xyz);
    }

    const std::vector<Point3d> boundary_xyz = outer_xyz;
    bool uv_complement = false;

    if (outer_uv.size() < 3)
    {
      BREP_WARN("tessellate: sphere face '{}' has <3 outer vertices", face.Name);
      continue;
    }

    std::vector<std::vector<Point2d>> holes_uv;
    holes_uv.reserve(region.Holes.size() + 1);
    for (const brep::mesh::SampledRing& hole : region.Holes)
    {
      std::vector<Point2d> uv;
      std::vector<Point3d> xyz;
      uv.reserve(hole.Points.size());
      xyz.reserve(hole.Points.size());
      for (const brep::mesh::SampledPoint& point : hole.Points)
      {
        uv.push_back(point.Uv);
        xyz.push_back(point.Xyz);
      }
      if (uv.size() < 3)
      {
        continue;
      }
      ensure_cw(uv, xyz);
      holes_uv.push_back(std::move(uv));
    }

    if (!is_analytic_sphere_seam_outer(face) &&
        trimmed_sphere_is_complement_patch(face))
    {
      uv_complement = true;
    }

    if (uv_complement)
    {
      std::vector<Point2d> cut = outer_uv;
      std::vector<Point3d> cut_xyz = outer_xyz;
      ensure_cw(cut, cut_xyz);
      holes_uv.insert(holes_uv.begin(), std::move(cut));

      double u_min = outer_uv.front().u();
      double u_max = u_min;
      for (const Point2d& p : outer_uv)
      {
        u_min = std::min(u_min, p.u());
        u_max = std::max(u_max, p.u());
      }
      double u0 = u_min;
      if (u_max - u_min < kTwoPi - 1e-9)
      {
        u0 = 0.5 * (u_min + u_max) - kPi;
      }
      outer_uv = {
          Point2d{u0, -0.5 * kPi},
          Point2d{u0, 0.5 * kPi},
          Point2d{u0 + kTwoPi, 0.5 * kPi},
          Point2d{u0 + kTwoPi, -0.5 * kPi},
      };
      outer_xyz = {
          sphere->Eval(normalize_sphere_u(outer_uv[0].u()), outer_uv[0].v()),
          sphere->Eval(normalize_sphere_u(outer_uv[1].u()), outer_uv[1].v()),
          sphere->Eval(normalize_sphere_u(outer_uv[2].u()), outer_uv[2].v()),
          sphere->Eval(normalize_sphere_u(outer_uv[3].u()), outer_uv[3].v()),
      };
    }

    ensure_ccw(outer_uv, outer_xyz);

    std::vector<Point2d> steiner =
        sphere_steiner_lattice(outer_uv, holes_uv, nu, nv);
    if (!is_analytic_sphere_seam_outer(face) && boundary_xyz.size() >= 3)
    {
      steiner.erase(
          std::remove_if(
              steiner.begin(), steiner.end(),
              [&](const Point2d& uv)
              {
                const Point3d p =
                    sphere->Eval(normalize_sphere_u(uv.u()), uv.v());
                return !point_on_spherical_face(
                    p, *sphere, boundary_xyz, uv_complement,
                    !is_sphere_imprint_patch(face));
              }),
          steiner.end());
    }

    const brep::mesh::CdtResult cdt =
        brep::mesh::triangulate_polygon_with_holes(outer_uv, holes_uv, steiner);
    if (!cdt.Ok)
    {
      BREP_WARN("tessellate: sphere CDT failed for face '{}': {}", face.Name,
                cdt.Diagnostics);
      const std::size_t indexBefore = mesh.Indices.size();
      if (is_analytic_sphere_seam_outer(face) && !face.InnerLoops().empty())
      {
        tessellate_sphere_seam_with_holes(face, *sphere, mesh, nu, nv,
                                        tessOpts);
      }
      if (mesh.Indices.size() <= indexBefore && boundary_xyz.size() >= 3)
      {
        tessellate_sphere_boundary_fan(face, *sphere, boundary_xyz, mesh);
      }
      continue;
    }

    double u_min = std::numeric_limits<double>::infinity();
    double u_max = -std::numeric_limits<double>::infinity();
    double v_min = std::numeric_limits<double>::infinity();
    double v_max = -std::numeric_limits<double>::infinity();
    for (const brep::mesh::CdtVertex& vertex : cdt.Vertices)
    {
      u_min = std::min(u_min, vertex.Uv.u());
      u_max = std::max(u_max, vertex.Uv.u());
      v_min = std::min(v_min, vertex.Uv.v());
      v_max = std::max(v_max, vertex.Uv.v());
    }
    const double du = std::max(u_max - u_min, 1e-9);
    const double dv = std::max(v_max - v_min, 1e-9);
    const std::uint32_t base =
        static_cast<std::uint32_t>(mesh.Vertices.size());

    for (const brep::mesh::CdtVertex& vertex : cdt.Vertices)
    {
      const double u_eval = normalize_sphere_u(vertex.Uv.u());
      const double v_eval = vertex.Uv.v();
      MeshVertex mesh_vertex;
      mesh_vertex.Position = sphere->Eval(u_eval, v_eval);
      mesh_vertex.Normal = face.NormalAt(u_eval, v_eval);
      mesh_vertex.Uv = Point2d{(vertex.Uv.u() - u_min) / du,
                               (vertex.Uv.v() - v_min) / dv};
      mesh.Vertices.push_back(mesh_vertex);
    }

    auto push_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
    {
      if (!is_analytic_sphere_seam_outer(face) && boundary_xyz.size() >= 3)
      {
        const Point3d& pa = mesh.Vertices[a].Position;
        const Point3d& pb = mesh.Vertices[b].Position;
        const Point3d& pc = mesh.Vertices[c].Position;
        const auto on_face = [&](const Point3d& p)
        {
          return point_on_spherical_face(
              p, *sphere, boundary_xyz, uv_complement,
              !is_sphere_imprint_patch(face));
        };
        if (!on_face(pa) && !on_face(pb) && !on_face(pc))
        {
          return;
        }
        if (is_sphere_imprint_patch(face))
        {
          const Point3d centroid = project_to_sphere_surface(
              Point3d{(pa.x() + pb.x() + pc.x()) / 3.0,
                      (pa.y() + pb.y() + pc.y()) / 3.0,
                      (pa.z() + pb.z() + pc.z()) / 3.0},
              sphere->Center(), sphere->Radius());
          if (!on_face(centroid))
          {
            return;
          }
        }
      }
      const Vector3d area =
          (mesh.Vertices[b].Position - mesh.Vertices[a].Position)
              .cross(mesh.Vertices[c].Position - mesh.Vertices[a].Position);
      if (area.squaredNorm() < 1e-24)
      {
        return;
      }
      if (area.dot(mesh.Vertices[a].Normal) < 0.0)
      {
        mesh.Indices.push_back(a);
        mesh.Indices.push_back(c);
        mesh.Indices.push_back(b);
      }
      else
      {
        mesh.Indices.push_back(a);
        mesh.Indices.push_back(b);
        mesh.Indices.push_back(c);
      }
    };

    for (const brep::mesh::CdtTriangle& triangle : cdt.Triangles)
    {
      push_tri(base + static_cast<std::uint32_t>(triangle.v[0]),
               base + static_cast<std::uint32_t>(triangle.v[1]),
               base + static_cast<std::uint32_t>(triangle.v[2]));
    }
  }
}

}  // namespace

TessellationOptions TessellationOptions::ForRadius(double radius)
{
  TessellationOptions opts;
  opts.LinearDeflection = std::max(0.02 * std::abs(radius), 1e-4);
  return opts;
}

void TessellateFace(const Face& face, TriangleMesh& out,
                     const TessellationOptions& opts)
{
  if (!face.Surface) return;
  switch (face.Surface->Kind())
  {
    case SurfaceKind::Sphere:
      tessellate_sphere_face(face, out, opts);
      break;
    case SurfaceKind::Plane:
      tessellate_plane_face(face, out, opts);
      break;
    default:
      BREP_WARN("TessellateFace: unsupported surface kind on '{}'", face.Name);
      break;
  }
}

TriangleMesh TessellateBody(const Body& body, const TessellationOptions& opts)
{
  AspectEvent event;
  event.Site = "mesh.tessellate";
  return ProcessAspectChain().Invoke(event, [&]() -> TriangleMesh {
  TriangleMesh mesh;

  if (body.Type == BodyType::Wire)
  {
    BREP_INFO("TessellateBody '{}': wire (no faces)", body.Name);
    return mesh;
  }

  for (const Shell* shell : body.Shells)
  {
    if (!shell) continue;
    bool hasCompanionSphere = false;
    for (const Face* face : shell->Faces)
    {
      if (face != nullptr && face->Surface != nullptr &&
          face->Surface->Kind() == SurfaceKind::Sphere &&
          !is_sphere_imprint_patch(*face))
      {
        hasCompanionSphere = true;
        break;
      }
    }
    for (const Face* face : shell->Faces)
    {
      if (!face) continue;
      // The inward octant patch sits on the same sphere as the surviving
      // ⅞ face and would fill the CSG hole. A circle cap is the other
      // body's kept surface and must be drawn.
      if (hasCompanionSphere && is_sphere_imprint_patch(*face) &&
          face->Name.find("_circle_imprint_cap") == std::string::npos)
      {
        continue;
      }
      TessellateFace(*face, mesh, opts);
    }
  }

  BREP_INFO("TessellateBody '{}': {} verts, {} tris", body.Name,
            mesh.Vertices.size(), mesh.Indices.size() / 3);
  return mesh;
  });
}

EdgeMesh ExtractEdges(const Body& body, const EdgeExtractionOptions& opts)
{
  EdgeMesh mesh;
  std::unordered_set<EdgeKey, EdgeKeyHash> seen;

  if (body.Type == BodyType::Wire)
  {
    const TessellationOptions tess_opts;
    for (const Edge* edge : body.WireEdges)
    {
      if (!edge || !edge->V0 || !edge->V1) continue;
      const EdgeKey key = make_edge_key(*edge);
      if (!seen.insert(key).second) continue;

      const std::vector<Point3d> samples =
          brep::mesh::SampleEdgeXyz(*edge, tess_opts);
      if (samples.size() < 2)
      {
        mesh.Positions.push_back(edge->V0->Position());
        mesh.Positions.push_back(edge->V1->Position());
        continue;
      }
      for (std::size_t i = 1; i < samples.size(); ++i)
      {
        mesh.Positions.push_back(samples[i - 1]);
        mesh.Positions.push_back(samples[i]);
      }
    }
    BREP_INFO("extract_edges '{}': {} segments (wire)", body.Name,
              mesh.Positions.size() / 2);
    return mesh;
  }

  for (const Shell* shell : body.Shells)
  {
    if (!shell) continue;
    for (const Face* face : shell->Faces)
    {
      if (!face) continue;
      for (const Loop* loop : face->Loops)
      {
        if (!loop) continue;
        loop->ForEachCoedge([&](const CoEdge& ce)
        {
          if (!ce.Edge || !ce.Edge->V0 || !ce.Edge->V1) return;
          if (!opts.IncludeSeamEdges && is_periodic_seam_edge(*ce.Edge))
          {
            return;
          }
          const EdgeKey key = make_edge_key(*ce.Edge);
          if (!seen.insert(key).second) return;

          const TessellationOptions tess_opts;
          const std::vector<Point3d> samples =
              brep::mesh::SampleEdgeXyz(ce, tess_opts);
          if (samples.size() < 2)
          {
            mesh.Positions.push_back(ce.Edge->V0->Position());
            mesh.Positions.push_back(ce.Edge->V1->Position());
            return;
          }
          for (std::size_t i = 1; i < samples.size(); ++i)
          {
            mesh.Positions.push_back(samples[i - 1]);
            mesh.Positions.push_back(samples[i]);
          }
        });
      }
    }
  }

  BREP_INFO("extract_edges '{}': {} segments", body.Name,
            mesh.Positions.size() / 2);
  return mesh;
}

}  // namespace brep
