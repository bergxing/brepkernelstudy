#include "brep/mesh.hpp"

#include "brep/geometry.hpp"
#include "brep/log.hpp"
#include "brep/mesh/cdt.hpp"
#include "brep/mesh/loop_sample.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace brep {
namespace {

struct EdgeKey {
  Id a;
  Id b;
  bool operator==(const EdgeKey& o) const noexcept { return a == o.a && b == o.b; }
};

struct EdgeKeyHash {
  std::size_t operator()(const EdgeKey& k) const noexcept {
    return (static_cast<std::size_t>(k.a) * 1315423911u) ^
           static_cast<std::size_t>(k.b);
  }
};

EdgeKey make_edge_key(const Edge& e) {
  const Id i0 = e.v0 ? e.v0->id : 0;
  const Id i1 = e.v1 ? e.v1->id : 0;
  return i0 < i1 ? EdgeKey{i0, i1} : EdgeKey{i1, i0};
}

/// Periodic seam: both radial coedges belong to the same face (e.g. sphere
/// meridian). Ordinary manifold edges have coedges on two different faces.
bool is_periodic_seam_edge(const Edge& e) {
  if (e.radial.size() != 2) return false;
  const CoEdge* a = e.radial[0];
  const CoEdge* b = e.radial[1];
  if (!a || !b || !a->loop || !b->loop) return false;
  return a->loop->face != nullptr && a->loop->face == b->loop->face;
}

int clamp_segments(int value, int lo, int hi) {
  return std::clamp(value, std::min(lo, hi), std::max(lo, hi));
}

[[nodiscard]] double ring_signed_area2d(const std::vector<Point2d>& ring) {
  double a = 0.0;
  const std::size_t n = ring.size();
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t j = (i + 1) % n;
    a += ring[i].u() * ring[j].v() - ring[j].u() * ring[i].v();
  }
  return 0.5 * a;
}

void ensure_ccw(std::vector<Point2d>& uv, std::vector<Point3d>& xyz) {
  if (ring_signed_area2d(uv) < 0.0) {
    std::reverse(uv.begin(), uv.end());
    std::reverse(xyz.begin(), xyz.end());
  }
}

void ensure_cw(std::vector<Point2d>& uv, std::vector<Point3d>& xyz) {
  if (ring_signed_area2d(uv) > 0.0) {
    std::reverse(uv.begin(), uv.end());
    std::reverse(xyz.begin(), xyz.end());
  }
}

[[nodiscard]] bool point_in_triangle3d(const Point3d& p, const Point3d& a,
                                       const Point3d& b, const Point3d& c,
                                       double eps = 1e-9) {
  const Vector3d n = (b - a).cross(c - a);
  if (n.squaredNorm() < 1e-24) return false;
  const Vector3d na = (b - a).cross(p - a);
  const Vector3d nb = (c - b).cross(p - b);
  const Vector3d nc = (a - c).cross(p - c);
  return na.dot(n) >= -eps && nb.dot(n) >= -eps && nc.dot(n) >= -eps;
}

void tessellate_plane_face(const Face& face, TriangleMesh& mesh,
                           const TessellationOptions& opts) {
  const auto* plane = dynamic_cast<const PlaneSurface*>(face.surface);
  if (!plane) {
    return;
  }

  for (const brep::mesh::FaceRegion& region :
       brep::mesh::group_face_regions(face, *plane, opts)) {
    std::vector<Point2d> outer_uv;
    std::vector<Point3d> outer_xyz;
    outer_uv.reserve(region.outer.points.size());
    outer_xyz.reserve(region.outer.points.size());
    for (const brep::mesh::SampledPoint& point : region.outer.points) {
      outer_uv.push_back(point.uv);
      outer_xyz.push_back(point.xyz);
    }
    if (outer_uv.size() < 3) {
      BREP_WARN("tessellate: face '{}' has <3 outer vertices", face.name);
      continue;
    }
    ensure_ccw(outer_uv, outer_xyz);

    std::vector<std::vector<Point2d>> holes_uv;
    holes_uv.reserve(region.holes.size());
    for (const brep::mesh::SampledRing& hole : region.holes) {
      std::vector<Point2d> uv;
      std::vector<Point3d> xyz;
      uv.reserve(hole.points.size());
      xyz.reserve(hole.points.size());
      for (const brep::mesh::SampledPoint& point : hole.points) {
        uv.push_back(point.uv);
        xyz.push_back(point.xyz);
      }
      if (uv.size() < 3) {
        continue;
      }
      ensure_cw(uv, xyz);
      holes_uv.push_back(std::move(uv));
    }

    const brep::mesh::CdtResult cdt =
        brep::mesh::triangulate_polygon_with_holes(outer_uv, holes_uv);
    if (!cdt.ok) {
      BREP_WARN("tessellate: CDT failed for face '{}': {}", face.name,
                cdt.diagnostics);
      continue;
    }

    double u_min = std::numeric_limits<double>::infinity();
    double u_max = -std::numeric_limits<double>::infinity();
    double v_min = std::numeric_limits<double>::infinity();
    double v_max = -std::numeric_limits<double>::infinity();
    for (const brep::mesh::CdtVertex& vertex : cdt.vertices) {
      u_min = std::min(u_min, vertex.uv.u());
      u_max = std::max(u_max, vertex.uv.u());
      v_min = std::min(v_min, vertex.uv.v());
      v_max = std::max(v_max, vertex.uv.v());
    }
    const double du = std::max(u_max - u_min, 1e-9);
    const double dv = std::max(v_max - v_min, 1e-9);
    const std::uint32_t base =
        static_cast<std::uint32_t>(mesh.vertices.size());
    for (const brep::mesh::CdtVertex& vertex : cdt.vertices) {
      MeshVertex mesh_vertex;
      mesh_vertex.position = plane->eval(vertex.uv.u(), vertex.uv.v());
      mesh_vertex.normal = face.normal_at(vertex.uv.u(), vertex.uv.v());
      mesh_vertex.uv = Point2d{(vertex.uv.u() - u_min) / du,
                               (vertex.uv.v() - v_min) / dv};
      mesh.vertices.push_back(mesh_vertex);
    }

    const bool flip_tris =
        plane->normal(0.0, 0.0).dot(face.normal_at(0.0, 0.0)) < 0.0;
    for (const brep::mesh::CdtTriangle& triangle : cdt.triangles) {
      const auto a = base + static_cast<std::uint32_t>(triangle.v[0]);
      const auto b = base + static_cast<std::uint32_t>(triangle.v[1]);
      const auto c = base + static_cast<std::uint32_t>(triangle.v[2]);
      if (!flip_tris) {
        mesh.indices.push_back(a);
        mesh.indices.push_back(b);
        mesh.indices.push_back(c);
      } else {
        mesh.indices.push_back(a);
        mesh.indices.push_back(c);
        mesh.indices.push_back(b);
      }
    }
  }
}

std::pair<int, int> sphere_segment_counts(double radius,
                                          const TessellationOptions& opts) {
  const double R = std::max(radius, 1e-12);
  const double h =
      opts.linear_deflection > 0.0
          ? opts.linear_deflection
          : std::max(0.02 * R, 1e-4);
  const double ang = std::max(opts.angular_deflection, 1e-6);

  int nu = static_cast<int>(std::ceil(2.0 * std::numbers::pi / ang));

  // Great-circle chord height h = R (1 - cos(α/2)) ⇒ α = 2 acos(1 - h/R).
  const double ratio = std::clamp(1.0 - h / R, -1.0, 1.0);
  const double alpha = 2.0 * std::acos(ratio);
  int nv_from_linear =
      alpha > 1e-12
          ? static_cast<int>(std::ceil(std::numbers::pi / alpha))
          : opts.max_v_segments;
  int nv_from_angular =
      static_cast<int>(std::ceil(std::numbers::pi / ang));
  int nv = std::max(nv_from_linear, nv_from_angular);

  nu = clamp_segments(nu, opts.min_u_segments, opts.max_u_segments);
  nv = clamp_segments(nv, opts.min_v_segments, opts.max_v_segments);
  return {nu, nv};
}

[[nodiscard]] double normalize_sphere_u(double u) {
  constexpr double kTwoPi = 2.0 * std::numbers::pi;
  double u_mod = std::fmod(u, kTwoPi);
  if (u_mod < 0.0) {
    u_mod += kTwoPi;
  }
  if (u_mod >= kTwoPi) {
    u_mod = 0.0;
  }
  return u_mod;
}

[[nodiscard]] bool point_in_polygon_uv(const Point2d& point,
                                       const std::vector<Point2d>& ring) {
  bool inside = false;
  for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    const Point2d& a = ring[i];
    const Point2d& b = ring[j];
    if ((a.v() > point.v()) != (b.v() > point.v()) &&
        point.u() < (b.u() - a.u()) * (point.v() - a.v()) / (b.v() - a.v()) +
                        a.u()) {
      inside = !inside;
    }
  }
  return inside;
}

[[nodiscard]] bool is_analytic_sphere_seam_outer(const Face& face) {
  const Loop* loop = face.outer_loop();
  if (!loop || loop->size() != 2) {
    return false;
  }
  const CoEdge* c0 = loop->first;
  const CoEdge* c1 = c0 ? c0->next : nullptr;
  return c0 && c1 && c0->edge != nullptr && c0->edge == c1->edge;
}

/// Three quarter-circle arcs: exterior ⅞-sphere patch (small loop bounds hole).
[[nodiscard]] bool is_trimmed_sphere_octant_outer(const Face& face) {
  const Loop* loop = face.outer_loop();
  if (!loop || loop->size() != 3) {
    return false;
  }
  bool all_circle = true;
  loop->for_each_coedge([&](const CoEdge& ce) {
    if (!ce.edge || !ce.edge->curve ||
        ce.edge->curve->kind() != CurveKind::Circle) {
      all_circle = false;
    }
  });
  return all_circle;
}

[[nodiscard]] std::vector<Point3d> spherical_loop_corners(
    const Point3d& center, const std::vector<Point3d>& boundary) {
  if (boundary.size() < 6) {
    return boundary;
  }
  std::array<std::size_t, 3> corners{};
  std::array<double, 3> turns{-1.0, -1.0, -1.0};
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const std::size_t im = (i + boundary.size() - 1) % boundary.size();
    const std::size_t ip = (i + 1) % boundary.size();
    const Vector3d dm = (boundary[im] - center).normalized();
    const Vector3d dp = (boundary[ip] - center).normalized();
    const double turn = dm.cross(dp).norm();
    for (int slot = 0; slot < 3; ++slot) {
      if (turn > turns[static_cast<std::size_t>(slot)]) {
        for (int j = 2; j > slot; --j) {
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
    const SphereSurface& sphere, const std::vector<Point3d>& boundary) {
  const Point3d& center = sphere.center();
  const double radius = sphere.radius();
  if (boundary.size() < 3) {
    return boundary.empty() ? center : boundary.front();
  }

  const std::vector<Point3d> corners = spherical_loop_corners(center, boundary);
  if (corners.size() == 3) {
    Vector3d sum{0, 0, 0};
    for (const Point3d& p : corners) {
      sum += p - center;
    }
    if (sum.squaredNorm() > 1e-24) {
      return center + sum.normalized() * radius;
    }
  }

  Vector3d sum{0, 0, 0};
  for (const Point3d& p : boundary) {
    sum += p - center;
  }
  if (sum.squaredNorm() < 1e-24) {
    return boundary.front();
  }
  return center + sum.normalized() * radius;
}

/// Great-circle polygon test on the sphere. `interior_hint` must lie inside the
/// bounded spherical polygon described by `boundary`.
[[nodiscard]] bool point_in_spherical_polygon(
    const Point3d& p, const Point3d& center,
    const std::vector<Point3d>& boundary, const Point3d& interior_hint) {
  if (boundary.size() < 3) {
    return false;
  }
  const Vector3d pd = p - center;
  const Vector3d hint = interior_hint - center;
  if (pd.squaredNorm() < 1e-24 || hint.squaredNorm() < 1e-24) {
    return false;
  }
  const Vector3d pn = pd.normalized();
  const Vector3d hn = hint.normalized();
  for (std::size_t i = 0; i < boundary.size(); ++i) {
    const Vector3d a = boundary[i] - center;
    const Vector3d b = boundary[(i + 1) % boundary.size()] - center;
    const Vector3d n = a.cross(b);
    if (n.squaredNorm() < 1e-24) {
      continue;
    }
    const Vector3d nn = n.normalized();
    if (pn.dot(nn) * hn.dot(nn) < 0.0) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool point_in_inward_spherical_octant(
    const Point3d& p, const Point3d& center, const Point3d& ax,
    const Point3d& ay, const Point3d& az, double eps = 1e-9) {
  const Vector3d vx = ax - center;
  const Vector3d vy = ay - center;
  const Vector3d vz = az - center;
  const Vector3d d = p - center;
  return d.dot(vx) >= -eps && d.dot(vy) >= -eps && d.dot(vz) >= -eps;
}

[[nodiscard]] bool point_on_spherical_face(
    const Point3d& p, const SphereSurface& sphere,
    const std::vector<Point3d>& boundary_xyz, bool uv_complement) {
  if (boundary_xyz.size() < 3) {
    return true;
  }
  if (uv_complement) {
    const std::vector<Point3d> corners =
        spherical_loop_corners(sphere.center(), boundary_xyz);
    if (corners.size() == 3) {
      const bool in_octant = point_in_inward_spherical_octant(
          p, sphere.center(), corners[0], corners[1], corners[2]);
      return !in_octant;
    }
  }
  const Point3d hint = spherical_polygon_interior_hint(sphere, boundary_xyz);
  const bool in_poly = point_in_spherical_polygon(
      p, sphere.center(), boundary_xyz, hint);
  return uv_complement ? !in_poly : in_poly;
}

/// True when the oriented face interior lies outside the UV polygon image of
/// the outer loop (large spherical patches such as a ⅞-ball).
[[nodiscard]] bool sphere_face_needs_uv_complement(
    const SphereSurface& sphere, const Face& face,
    const std::vector<Point2d>& outer_uv,
    const std::vector<Point3d>& outer_xyz) {
  if (outer_uv.size() < 3 || outer_uv.size() != outer_xyz.size()) {
    return false;
  }
  const Point3d& center = sphere.center();
  const double radius = sphere.radius();
  constexpr double kPi = std::numbers::pi;

  for (std::size_t i = 0; i < outer_xyz.size(); ++i) {
    const std::size_t j = (i + 1) % outer_xyz.size();
    const Point3d& a = outer_xyz[i];
    const Point3d& b = outer_xyz[j];
    Vector3d tangent = b - a;
    if (tangent.squaredNorm() < 1e-18) {
      continue;
    }
    tangent = tangent.normalized();

    const Point2d umid{(outer_uv[i].u() + outer_uv[j].u()) * 0.5,
                       (outer_uv[i].v() + outer_uv[j].v()) * 0.5};
    const double u_eval = normalize_sphere_u(umid.u());
    const double v_eval =
        std::clamp(umid.v(), -0.5 * kPi + 1e-6, 0.5 * kPi - 1e-6);
    const Point3d mid = sphere.eval(u_eval, v_eval);
    const Vector3d normal = face.normal_at(u_eval, v_eval);
    // Face interior lies to the left of the coedge when the head is along the
    // face normal: tangent × normal (right-handed).
    Vector3d into_face = tangent.cross(normal);
    if (into_face.squaredNorm() < 1e-18) {
      continue;
    }
    into_face = into_face.normalized();

    const Point3d probed_raw = mid + into_face * (0.02 * radius);
    const Vector3d dir = probed_raw - center;
    if (dir.squaredNorm() < 1e-18) {
      continue;
    }
    const Point3d on_sphere = center + dir.normalized() * radius;
    Point2d probe_uv = sphere.param_of(on_sphere);
    while (probe_uv.u() - umid.u() > kPi) {
      probe_uv.u() -= 2.0 * kPi;
    }
    while (probe_uv.u() - umid.u() < -kPi) {
      probe_uv.u() += 2.0 * kPi;
    }
    // Align v near mid (poles are noisy).
    if (std::abs(probe_uv.v() - umid.v()) > 0.5 * kPi) {
      continue;
    }
    return !point_in_polygon_uv(probe_uv, outer_uv);
  }
  return false;
}

[[nodiscard]] std::vector<Point2d> sphere_steiner_lattice(
    const std::vector<Point2d>& outer_ccw,
    const std::vector<std::vector<Point2d>>& holes_cw, int nu, int nv) {
  if (outer_ccw.empty() || nu < 2 || nv < 2) {
    return {};
  }
  double u_min = outer_ccw.front().u();
  double u_max = u_min;
  double v_min = outer_ccw.front().v();
  double v_max = v_min;
  for (const Point2d& p : outer_ccw) {
    u_min = std::min(u_min, p.u());
    u_max = std::max(u_max, p.u());
    v_min = std::min(v_min, p.v());
    v_max = std::max(v_max, p.v());
  }
  const double du = u_max - u_min;
  const double dv = v_max - v_min;
  if (du < 1e-12 || dv < 1e-12) {
    return {};
  }

  std::vector<Point2d> steiner;
  steiner.reserve(static_cast<std::size_t>(nu - 1) *
                  static_cast<std::size_t>(nv - 1));
  for (int iv = 1; iv < nv; ++iv) {
    const double v =
        v_min + (static_cast<double>(iv) / static_cast<double>(nv)) * dv;
    for (int iu = 1; iu < nu; ++iu) {
      const double u =
          u_min + (static_cast<double>(iu) / static_cast<double>(nu)) * du;
      const Point2d p{u, v};
      if (!point_in_polygon_uv(p, outer_ccw)) {
        continue;
      }
      bool in_hole = false;
      for (const auto& hole : holes_cw) {
        if (point_in_polygon_uv(p, hole)) {
          in_hole = true;
          break;
        }
      }
      if (!in_hole) {
        steiner.push_back(p);
      }
    }
  }
  return steiner;
}

void tessellate_sphere_face(const Face& face, TriangleMesh& mesh,
                            const TessellationOptions& opts) {
  const auto* sphere = dynamic_cast<const SphereSurface*>(face.surface);
  if (!sphere) {
    return;
  }
  if (!face.outer_loop()) {
    BREP_WARN("tessellate: sphere face '{}' has no outer loop", face.name);
    return;
  }

  constexpr double kPi = std::numbers::pi;
  constexpr double kTwoPi = 2.0 * kPi;
  const auto [nu, nv] = sphere_segment_counts(sphere->radius(), opts);

  for (brep::mesh::FaceRegion region :
       brep::mesh::group_face_regions(face, *sphere, opts)) {
    region.outer = brep::mesh::unwrap_sphere_ring(std::move(region.outer));
    for (brep::mesh::SampledRing& hole : region.holes) {
      hole = brep::mesh::unwrap_sphere_ring(std::move(hole));
    }

    std::vector<Point2d> outer_uv;
    std::vector<Point3d> outer_xyz;
    outer_uv.reserve(region.outer.points.size());
    outer_xyz.reserve(region.outer.points.size());
    for (const brep::mesh::SampledPoint& point : region.outer.points) {
      outer_uv.push_back(point.uv);
      outer_xyz.push_back(point.xyz);
    }

    const std::vector<Point3d> boundary_xyz = outer_xyz;
    bool uv_complement = false;

    if (is_analytic_sphere_seam_outer(face)) {
      outer_uv = {
          Point2d{0.0, -0.5 * kPi},
          Point2d{0.0, 0.5 * kPi},
          Point2d{kTwoPi, 0.5 * kPi},
          Point2d{kTwoPi, -0.5 * kPi},
      };
      outer_xyz = {
          sphere->eval(0.0, -0.5 * kPi),
          sphere->eval(0.0, 0.5 * kPi),
          sphere->eval(0.0, 0.5 * kPi),
          sphere->eval(0.0, -0.5 * kPi),
      };
    } else if (outer_uv.size() < 3) {
      BREP_WARN("tessellate: sphere face '{}' has <3 outer vertices", face.name);
      continue;
    }

    std::vector<std::vector<Point2d>> holes_uv;
    holes_uv.reserve(region.holes.size() + 1);
    for (const brep::mesh::SampledRing& hole : region.holes) {
      std::vector<Point2d> uv;
      std::vector<Point3d> xyz;
      uv.reserve(hole.points.size());
      xyz.reserve(hole.points.size());
      for (const brep::mesh::SampledPoint& point : hole.points) {
        uv.push_back(point.uv);
        xyz.push_back(point.xyz);
      }
      if (uv.size() < 3) {
        continue;
      }
      ensure_cw(uv, xyz);
      holes_uv.push_back(std::move(uv));
    }

    if (!is_analytic_sphere_seam_outer(face) &&
        (sphere_face_needs_uv_complement(*sphere, face, outer_uv, outer_xyz) ||
         is_trimmed_sphere_octant_outer(face))) {
      uv_complement = true;
      std::vector<Point2d> cut = outer_uv;
      std::vector<Point3d> cut_xyz = outer_xyz;
      ensure_cw(cut, cut_xyz);
      holes_uv.insert(holes_uv.begin(), std::move(cut));

      double u_min = outer_uv.front().u();
      double u_max = u_min;
      for (const Point2d& p : outer_uv) {
        u_min = std::min(u_min, p.u());
        u_max = std::max(u_max, p.u());
      }
      double u0 = u_min;
      if (u_max - u_min < kTwoPi - 1e-9) {
        u0 = 0.5 * (u_min + u_max) - kPi;
      }
      outer_uv = {
          Point2d{u0, -0.5 * kPi},
          Point2d{u0, 0.5 * kPi},
          Point2d{u0 + kTwoPi, 0.5 * kPi},
          Point2d{u0 + kTwoPi, -0.5 * kPi},
      };
      outer_xyz = {
          sphere->eval(normalize_sphere_u(outer_uv[0].u()), outer_uv[0].v()),
          sphere->eval(normalize_sphere_u(outer_uv[1].u()), outer_uv[1].v()),
          sphere->eval(normalize_sphere_u(outer_uv[2].u()), outer_uv[2].v()),
          sphere->eval(normalize_sphere_u(outer_uv[3].u()), outer_uv[3].v()),
      };
    }

    ensure_ccw(outer_uv, outer_xyz);

    std::vector<Point2d> steiner =
        sphere_steiner_lattice(outer_uv, holes_uv, nu, nv);
    if (!is_analytic_sphere_seam_outer(face) && boundary_xyz.size() >= 3) {
      steiner.erase(
          std::remove_if(
              steiner.begin(), steiner.end(),
              [&](const Point2d& uv) {
                const Point3d p =
                    sphere->eval(normalize_sphere_u(uv.u()), uv.v());
                return !point_on_spherical_face(p, *sphere, boundary_xyz,
                                                uv_complement);
              }),
          steiner.end());
    }

    const brep::mesh::CdtResult cdt =
        brep::mesh::triangulate_polygon_with_holes(outer_uv, holes_uv, steiner);
    if (!cdt.ok) {
      BREP_WARN("tessellate: sphere CDT failed for face '{}': {}", face.name,
                cdt.diagnostics);
      continue;
    }

    double u_min = std::numeric_limits<double>::infinity();
    double u_max = -std::numeric_limits<double>::infinity();
    double v_min = std::numeric_limits<double>::infinity();
    double v_max = -std::numeric_limits<double>::infinity();
    for (const brep::mesh::CdtVertex& vertex : cdt.vertices) {
      u_min = std::min(u_min, vertex.uv.u());
      u_max = std::max(u_max, vertex.uv.u());
      v_min = std::min(v_min, vertex.uv.v());
      v_max = std::max(v_max, vertex.uv.v());
    }
    const double du = std::max(u_max - u_min, 1e-9);
    const double dv = std::max(v_max - v_min, 1e-9);
    const std::uint32_t base =
        static_cast<std::uint32_t>(mesh.vertices.size());

    std::optional<Point3d> inward_octant_probe;
    if (uv_complement && boundary_xyz.size() >= 3) {
      const std::vector<Point3d> corners =
          spherical_loop_corners(sphere->center(), boundary_xyz);
      if (corners.size() == 3) {
        const Point3d& sc = sphere->center();
        const Vector3d sum = (corners[0] - sc) + (corners[1] - sc) +
                             (corners[2] - sc);
        if (sum.squaredNorm() > 1e-24) {
          inward_octant_probe = sc + sum.normalized() * sphere->radius();
        }
      }
    }

    for (const brep::mesh::CdtVertex& vertex : cdt.vertices) {
      const double u_eval = normalize_sphere_u(vertex.uv.u());
      const double v_eval = vertex.uv.v();
      MeshVertex mesh_vertex;
      mesh_vertex.position = sphere->eval(u_eval, v_eval);
      mesh_vertex.normal = face.normal_at(u_eval, v_eval);
      mesh_vertex.uv = Point2d{(vertex.uv.u() - u_min) / du,
                               (vertex.uv.v() - v_min) / dv};
      mesh.vertices.push_back(mesh_vertex);
    }

    auto push_tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
      if (!is_analytic_sphere_seam_outer(face) && boundary_xyz.size() >= 3) {
        const Point3d& pa = mesh.vertices[a].position;
        const Point3d& pb = mesh.vertices[b].position;
        const Point3d& pc = mesh.vertices[c].position;
        const Point3d centroid{
            (pa.x() + pb.x() + pc.x()) / 3.0,
            (pa.y() + pb.y() + pc.y()) / 3.0,
            (pa.z() + pb.z() + pc.z()) / 3.0,
        };
        if (!point_on_spherical_face(centroid, *sphere, boundary_xyz,
                                     uv_complement) ||
            !point_on_spherical_face(pa, *sphere, boundary_xyz, uv_complement) ||
            !point_on_spherical_face(pb, *sphere, boundary_xyz, uv_complement) ||
            !point_on_spherical_face(pc, *sphere, boundary_xyz, uv_complement)) {
          return;
        }
        if (inward_octant_probe.has_value() &&
            point_in_triangle3d(*inward_octant_probe, pa, pb, pc, 1e-6)) {
          return;
        }
      }
      const Vector3d area =
          (mesh.vertices[b].position - mesh.vertices[a].position)
              .cross(mesh.vertices[c].position - mesh.vertices[a].position);
      if (area.squaredNorm() < 1e-24) {
        return;
      }
      if (area.dot(mesh.vertices[a].normal) < 0.0) {
        mesh.indices.push_back(a);
        mesh.indices.push_back(c);
        mesh.indices.push_back(b);
      } else {
        mesh.indices.push_back(a);
        mesh.indices.push_back(b);
        mesh.indices.push_back(c);
      }
    };

    for (const brep::mesh::CdtTriangle& triangle : cdt.triangles) {
      push_tri(base + static_cast<std::uint32_t>(triangle.v[0]),
               base + static_cast<std::uint32_t>(triangle.v[1]),
               base + static_cast<std::uint32_t>(triangle.v[2]));
    }
  }
}

}  // namespace

TessellationOptions TessellationOptions::for_radius(double radius) {
  TessellationOptions opts;
  opts.linear_deflection = std::max(0.02 * std::abs(radius), 1e-4);
  return opts;
}

void tessellate_face(const Face& face, TriangleMesh& out,
                     const TessellationOptions& opts) {
  if (!face.surface) return;
  switch (face.surface->kind()) {
    case SurfaceKind::Sphere:
      tessellate_sphere_face(face, out, opts);
      break;
    case SurfaceKind::Plane:
      tessellate_plane_face(face, out, opts);
      break;
    default:
      BREP_WARN("tessellate_face: unsupported surface kind on '{}'", face.name);
      break;
  }
}

TriangleMesh tessellate_body(const Body& body, const TessellationOptions& opts) {
  TriangleMesh mesh;

  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (const Face* face : shell->faces) {
      if (!face) continue;
      tessellate_face(*face, mesh, opts);
    }
  }

  BREP_INFO("tessellate_body '{}': {} verts, {} tris", body.name,
            mesh.vertices.size(), mesh.indices.size() / 3);
  return mesh;
}

EdgeMesh extract_edges(const Body& body, const EdgeExtractionOptions& opts) {
  EdgeMesh mesh;
  std::unordered_set<EdgeKey, EdgeKeyHash> seen;

  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (const Face* face : shell->faces) {
      if (!face) continue;
      for (const Loop* loop : face->loops) {
        if (!loop) continue;
        loop->for_each_coedge([&](const CoEdge& ce) {
          if (!ce.edge || !ce.edge->v0 || !ce.edge->v1) return;
          if (!opts.include_seam_edges && is_periodic_seam_edge(*ce.edge)) {
            return;
          }
          const EdgeKey key = make_edge_key(*ce.edge);
          if (!seen.insert(key).second) return;
          mesh.positions.push_back(ce.edge->v0->position());
          mesh.positions.push_back(ce.edge->v1->position());
        });
      }
    }
  }

  BREP_INFO("extract_edges '{}': {} segments", body.name,
            mesh.positions.size() / 2);
  return mesh;
}

}  // namespace brep
