#include "brep/bool/BoxBoolean.h"

#include "brep/bool/BoxRecognize.h"
#include "brep/Builder.h"
#include "brep/Geometry.h"
#include "brep/Log.h"
#include "brep/Validate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace brep::boolean
{
namespace
{

struct Aabb
{
  Point3d Min{};
  Point3d Max{};

  [[nodiscard]] bool contains_point(const Point3d& p, double eps) const
  {
    return p.x() >= Min.x() - eps && p.x() <= Max.x() + eps &&
           p.y() >= Min.y() - eps && p.y() <= Max.y() + eps &&
           p.z() >= Min.z() - eps && p.z() <= Max.z() + eps;
  }

  [[nodiscard]] bool contains_cell_center(const Point3d& c0, const Point3d& c1,
                                          double /*eps*/) const
  {
    const Point3d mid{(c0.x() + c1.x()) * 0.5, (c0.y() + c1.y()) * 0.5,
                      (c0.z() + c1.z()) * 0.5};
    // Strict interior-or-boundary relative to open max faces of half-open grid.
    return mid.x() >= Min.x() && mid.x() <= Max.x() && mid.y() >= Min.y() &&
           mid.y() <= Max.y() && mid.z() >= Min.z() && mid.z() <= Max.z();
  }
};

[[nodiscard]] Aabb from_spec(const BoxSpec& s)
{
  return Aabb{s.Min, s.Max};
}

void merge_unique(std::vector<double>& xs, double v, double eps)
{
  for (double x : xs)
{
    if (std::abs(x - v) <= eps) return;
  }
  xs.push_back(v);
}

void sort_unique(std::vector<double>& xs, double eps)
{
  std::sort(xs.begin(), xs.end());
  std::vector<double> out;
  out.reserve(xs.size());
  for (double v : xs)
  {
    if (out.empty() || std::abs(out.back() - v) > eps) out.push_back(v);
  }
  xs.swap(out);
}

struct CellKey
  {
  int i;
  int j;
  int k;
  bool operator<(const CellKey& o) const noexcept
  {
    if (i != o.i) return i < o.i;
    if (j != o.j) return j < o.j;
    return k < o.k;
  }
};

[[nodiscard]] bool keep_cell(BooleanOp op, bool in_a, bool in_b)
{
  switch (op)
{
    case BooleanOp::Union:
      return in_a || in_b;
    case BooleanOp::Subtract:
      return in_a && !in_b;
    case BooleanOp::Intersect:
      return in_a && in_b;
  }
  return false;
}

struct FaceQuad
{
    std::array<Point3d, 4> Corners;  // CCW when viewed against outward normal
    Vector3d Normal;
};

void emit_cell_faces(const Point3d& c0, const Point3d& c1,
                     bool nx_neg, bool nx_pos, bool ny_neg, bool ny_pos,
                     bool nz_neg, bool nz_pos, std::vector<FaceQuad>& out)
                     {
  const double x0 = c0.x(), x1 = c1.x();
  const double y0 = c0.y(), y1 = c1.y();
  const double z0 = c0.z(), z1 = c1.z();

  auto push = [&](Point3d a, Point3d b, Point3d c, Point3d d, Vector3d n)
  {
    out.push_back(FaceQuad{{a, b, c, d}, n});
  };

  // Outward faces: CCW when looking along -normal (from outside).
  if (nx_neg)
  {
      // -X
    push({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {-1, 0, 0});
  }
  if (nx_pos)
  {
      // +X
    push({x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, {1, 0, 0});
  }
  if (ny_neg)
  {
      // -Y
    push({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0, -1, 0});
  }
  if (ny_pos)
  {
      // +Y
    push({x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {0, 1, 0});
  }
  if (nz_neg)
  {
      // -Z
    push({x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, {0, 0, -1});
  }
  if (nz_pos)
  {
      // +Z
    push({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1});
  }
}

struct VertKey
  {
  long long x;
  long long y;
  long long z;
  bool operator<(const VertKey& o) const noexcept
  {
    if (x != o.x) return x < o.x;
    if (y != o.y) return y < o.y;
    return z < o.z;
  }
};

[[nodiscard]] VertKey quantize(const Point3d& p, double scale)
{
  return VertKey{llround(p.x() * scale), llround(p.y() * scale),
                 llround(p.z() * scale)};
}

struct EdgeKey
{
  VertKey a;
  VertKey b;
  bool operator<(const EdgeKey& o) const noexcept
  {
    if (a.x != o.a.x) return a.x < o.a.x;
    if (a.y != o.a.y) return a.y < o.a.y;
    if (a.z != o.a.z) return a.z < o.a.z;
    if (b.x != o.b.x) return b.x < o.b.x;
    if (b.y != o.b.y) return b.y < o.b.y;
    return b.z < o.b.z;
  }
};

[[nodiscard]] EdgeKey make_edge_key(VertKey a, VertKey b)
{
  if (b < a) std::swap(a, b);
  return EdgeKey{a, b};
}

Body* build_from_quads(Model& model, const std::vector<FaceQuad>& quads,
                       const std::string& name, double tol)
{
  if (quads.empty()) return nullptr;

  Body* body = model.MakeBody(BodyType::Solid, name);
  Shell* shell = model.MakeShell(true, name + "_shell");
  body->Shells.push_back(shell);

  constexpr double kScale = 1e9;
  std::map<VertKey, Vertex*> verts;
  std::map<EdgeKey, Edge*> edges;

  auto get_vert = [&](const Point3d& p) -> Vertex* {
    const VertKey key = quantize(p, kScale);
    if (auto it = verts.find(key); it != verts.end()) return it->second;
    Point* pt = model.MakePoint(p);
    Vertex* v = model.MakeVertex(pt, tol);
    verts.emplace(key, v);
    return v;
  };

  auto get_edge = [&](Vertex* va, Vertex* vb) -> std::pair<Edge*, bool> {
    const VertKey ka = quantize(va->Position(), kScale);
    const VertKey kb = quantize(vb->Position(), kScale);
    const EdgeKey key = make_edge_key(ka, kb);
    if (auto it = edges.find(key); it != edges.end())
    {
      return {it->second, it->second->V0 == va};
    }
    LineCurve* curve = model.MakeLine(va->Position(), vb->Position());
    Edge* e =
        model.MakeEdge(curve, va, vb, 0.0, curve->Length(), tol);
    edges.emplace(key, e);
    return {e, true};
  };

  int face_i = 0;
  for (const FaceQuad& q : quads)
  {
    Vertex* vv[4] = {get_vert(q.Corners[0]), get_vert(q.Corners[1]),
                     get_vert(q.Corners[2]), get_vert(q.Corners[3])};

    // Build orthonormal UV from first edge and normal.
    Vector3d u = (q.Corners[1] - q.Corners[0]);
    if (u.norm() < 1e-18) continue;
    u = u.normalized();
    Vector3d n = q.Normal.normalized();
    Vector3d v = n.cross(u).normalized();
    // Ensure (u,v,n) right-handed with n outward.
    if (u.cross(v).dot(n) < 0.0) v = -v;

    PlaneSurface* surf =
        model.MakePlane(q.Corners[0], u, v, name + "_f" + std::to_string(face_i));
    Face* face = model.MakeFace(surf, Orientation::Forward,
                                 name + "_face" + std::to_string(face_i));
    shell->Faces.push_back(face);
    Loop* loop =
        model.MakeLoop(face, LoopType::Outer, name + "_loop" + std::to_string(face_i));

    std::array<CoEdge*, 4> ces{};
    for (int k = 0; k < 4; ++k)
    {
      Vertex* a = vv[k];
      Vertex* b = vv[(k + 1) % 4];
      auto [edge, forward] = get_edge(a, b);
      const Orientation sense =
          forward ? Orientation::Forward : Orientation::Reversed;
      const Point2d uva{surf->ParamOf(a->Position())};
      const Point2d uvb{surf->ParamOf(b->Position())};
      Curve2d* pc = model.MakeLine2d(uva, uvb);
      ces[k] = model.MakeCoedge(edge, sense, pc);
    }
    Model::LinkLoop(loop, ces);
    ++face_i;
  }

  // Pair partners for manifold edges.
  for (auto& [key, edge] : edges)
  {
    (void)key;
    if (edge->Radial.size() != 2)
    {
      BREP_ERROR("box_boolean: edge radial degree={} (expected 2)",
                 edge->Radial.size());
      return nullptr;
    }
    Model::PairPartners(edge->Radial[0], edge->Radial[1]);
  }

  const auto report = ValidateBody(*body);
  if (!report.Ok())
  {
    BREP_WARN("box_boolean: validate failed after build");
    for (const auto& issue : report.Issues)
    {
      BREP_WARN("  [{}] {}", issue.Where, issue.Message);
    }
    return nullptr;
  }
  return body;
}

[[nodiscard]] const char* op_name(BooleanOp op) noexcept
{
  switch (op)
{
    case BooleanOp::Union:
      return "Union";
    case BooleanOp::Subtract:
      return "Subtract";
    case BooleanOp::Intersect:
      return "Intersect";
  }
  return "Unknown";
}

}  // namespace

BooleanResult EvaluateBoxBoolean(BooleanOp op, Model& model, const Body& a,
                                   const Body& b, const BooleanContext& ctx)
{
  BooleanResult result;
  result.Mode = BooleanEvalMode::AnalyticPair;

  const auto sa = RecognizeAxisAlignedBox(a, ctx);
  const auto sb = RecognizeAxisAlignedBox(b, ctx);
  if (!sa || !sb)
  {
    result.Diagnostics =
        std::string("boolean: unsupported combination for ") + op_name(op) +
        " (axis-aligned boxes required)";
    return result;
  }

  const double eps = std::max(ctx.fuzzy, 1e-12);
  const Aabb aa = from_spec(*sa);
  const Aabb bb = from_spec(*sb);

  std::vector<double> xs, ys, zs;
  merge_unique(xs, aa.Min.x(), eps);
  merge_unique(xs, aa.Max.x(), eps);
  merge_unique(xs, bb.Min.x(), eps);
  merge_unique(xs, bb.Max.x(), eps);
  merge_unique(ys, aa.Min.y(), eps);
  merge_unique(ys, aa.Max.y(), eps);
  merge_unique(ys, bb.Min.y(), eps);
  merge_unique(ys, bb.Max.y(), eps);
  merge_unique(zs, aa.Min.z(), eps);
  merge_unique(zs, aa.Max.z(), eps);
  merge_unique(zs, bb.Min.z(), eps);
  merge_unique(zs, bb.Max.z(), eps);
  sort_unique(xs, eps);
  sort_unique(ys, eps);
  sort_unique(zs, eps);

  const int nx = static_cast<int>(xs.size()) - 1;
  const int ny = static_cast<int>(ys.size()) - 1;
  const int nz = static_cast<int>(zs.size()) - 1;
  if (nx <= 0 || ny <= 0 || nz <= 0)
  {
    result.Diagnostics = "boolean: degenerate arrangement";
    return result;
  }

  auto solid = [&](int i, int j, int k) -> bool {
    if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return false;
    const Point3d c0{xs[i], ys[j], zs[k]};
    const Point3d c1{xs[i + 1], ys[j + 1], zs[k + 1]};
    const bool in_a = aa.contains_cell_center(c0, c1, eps);
    const bool in_b = bb.contains_cell_center(c0, c1, eps);
    return keep_cell(op, in_a, in_b);
  };

  std::vector<FaceQuad> quads;
  int solid_count = 0;
  for (int i = 0; i < nx; ++i)
  {
    for (int j = 0; j < ny; ++j)
  {
      for (int k = 0; k < nz; ++k)
  {
        if (!solid(i, j, k)) continue;
        ++solid_count;
        const Point3d c0{xs[i], ys[j], zs[k]};
        const Point3d c1{xs[i + 1], ys[j + 1], zs[k + 1]};
        emit_cell_faces(c0, c1, !solid(i - 1, j, k), !solid(i + 1, j, k),
                        !solid(i, j - 1, k), !solid(i, j + 1, k),
                        !solid(i, j, k - 1), !solid(i, j, k + 1), quads);
      }
    }
  }

  if (solid_count == 0 || quads.empty())
  {
    result.Diagnostics =
        std::string("boolean ") + op_name(op) + ": empty result";
    BREP_WARN("{}", result.Diagnostics);
    return result;
  }

  // Fast path: single AABB result �?reuse MakeBox.
  if (solid_count == nx * ny * nz ||
      (nx == 1 && ny == 1 && nz == 1 && solid_count == 1))
  {
    // Check if solid cells form one AABB by scanning bounds of solid cells.
  }

  Point3d rmin{1e300, 1e300, 1e300};
  Point3d rmax{-1e300, -1e300, -1e300};
  int solid_again = 0;
  bool is_filled_aabb = true;
  for (int i = 0; i < nx; ++i)
  {
    for (int j = 0; j < ny; ++j)
  {
      for (int k = 0; k < nz; ++k)
  {
        if (!solid(i, j, k))
  {
          // hole in arrangement bbox of solids �?checked after
          continue;
        }
        ++solid_again;
        rmin = Point3d{std::min(rmin.x(), xs[i]), std::min(rmin.y(), ys[j]),
                       std::min(rmin.z(), zs[k])};
        rmax = Point3d{std::max(rmax.x(), xs[i + 1]),
                       std::max(rmax.y(), ys[j + 1]),
                       std::max(rmax.z(), zs[k + 1])};
      }
    }
  }
  // Count how many arrangement cells lie inside [rmin,rmax] and are solid.
  int expect = 0;
  int got = 0;
  for (int i = 0; i < nx; ++i)
  {
    for (int j = 0; j < ny; ++j)
  {
      for (int k = 0; k < nz; ++k)
  {
        const Point3d mid{(xs[i] + xs[i + 1]) * 0.5, (ys[j] + ys[j + 1]) * 0.5,
                          (zs[k] + zs[k + 1]) * 0.5};
        if (mid.x() < rmin.x() || mid.x() > rmax.x() || mid.y() < rmin.y() ||
            mid.y() > rmax.y() || mid.z() < rmin.z() || mid.z() > rmax.z())
        {
          continue;
        }
        // Cell overlaps result AABB interior.
        const bool overlaps =
            xs[i + 1] > rmin.x() + eps && xs[i] < rmax.x() - eps &&
            ys[j + 1] > rmin.y() + eps && ys[j] < rmax.y() - eps &&
            zs[k + 1] > rmin.z() + eps && zs[k] < rmax.z() - eps;
        if (!overlaps) continue;
        ++expect;
        if (solid(i, j, k)) ++got;
      }
    }
  }
  is_filled_aabb = (expect > 0 && expect == got && solid_again == got);

  const std::string out_name =
      std::string("bool_") + a.Name + "_" + b.Name + "_" + op_name(op);

  if (is_filled_aabb)
  {
    BoxSpec spec;
    spec.Min = rmin;
    spec.Max = rmax;
    spec.Tolerance = std::max(sa->Tolerance, sb->Tolerance);
    spec.Name = out_name;
    result.OutputBody = MakeBox(model, spec);
    result.Diagnostics.clear();
    return result;
  }

  result.Mode = BooleanEvalMode::General;
  result.OutputBody = build_from_quads(model, quads, out_name,
                                 std::max(sa->Tolerance, sb->Tolerance));
  if (!result.OutputBody)
  {
    result.Diagnostics =
        std::string("boolean ") + op_name(op) + ": failed to build manifold shell";
    BREP_WARN("{}", result.Diagnostics);
  }
  return result;
}

}  // namespace brep::boolean
