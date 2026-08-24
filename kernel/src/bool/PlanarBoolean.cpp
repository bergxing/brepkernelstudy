#include "brep/bool/PlanarBoolean.h"

#include "brep/bool/Classify.h"
#include "brep/Builder.h"
#include "brep/Geometry.h"
#include "brep/Log.h"
#include "brep/Validate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace brep::boolean
{
namespace
{

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
    std::array<Point3d, 4> Corners;
    Vector3d Normal;
};

void emit_cell_faces(const Point3d& c0, const Point3d& c1, bool nx_neg,
                     bool nx_pos, bool ny_neg, bool ny_pos, bool nz_neg,
                     bool nz_pos, std::vector<FaceQuad>& out)
                     {
  const double x0 = c0.x(), x1 = c1.x();
  const double y0 = c0.y(), y1 = c1.y();
  const double z0 = c0.z(), z1 = c1.z();

  auto push = [&](Point3d a, Point3d b, Point3d c, Point3d d, Vector3d n)
  {
    out.push_back(FaceQuad{{a, b, c, d}, n});
  };

  if (nx_neg)
  {
    push({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {-1, 0, 0});
  }
  if (nx_pos)
  {
    push({x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}, {1, 0, 0});
  }
  if (ny_neg)
  {
    push({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0, -1, 0});
  }
  if (ny_pos)
  {
    push({x0, y1, z0}, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {0, 1, 0});
  }
  if (nz_neg)
  {
    push({x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, {0, 0, -1});
  }
  if (nz_pos)
  {
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
    Edge* e = model.MakeEdge(curve, va, vb, 0.0, curve->Length(), tol);
    edges.emplace(key, e);
    return {e, true};
  };

  int face_i = 0;
  for (const FaceQuad& q : quads)
  {
    Vertex* vv[4] = {get_vert(q.Corners[0]), get_vert(q.Corners[1]),
                     get_vert(q.Corners[2]), get_vert(q.Corners[3])};

    Vector3d u = (q.Corners[1] - q.Corners[0]);
    if (u.norm() < 1e-18) continue;
    u = u.normalized();
    Vector3d n = q.Normal.normalized();
    Vector3d v = n.cross(u).normalized();
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

  for (auto& [key, edge] : edges)
  {
    (void)key;
    if (edge->Radial.size() != 2)
    {
      BREP_ERROR("planar_boolean: edge radial degree={} (expected 2)",
                 edge->Radial.size());
      return nullptr;
    }
    Model::PairPartners(edge->Radial[0], edge->Radial[1]);
  }

  if (!ValidateBody(*body).Ok()) return nullptr;
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

void collect_profile_coords(const PlanarPrismSpec& prism,
                            std::vector<double>& xs, std::vector<double>& ys,
                            std::vector<double>& zs, double eps)
                            {
  auto add_uv = [&](const Point2d& uv, double h)
                            {
    const Point3d p =
        prism.Plane.Origin + prism.Plane.UAxis * uv.u() +
        prism.Plane.VAxis * uv.v() + prism.Plane.Normal * h;
    merge_unique(xs, p.x(), eps);
    merge_unique(ys, p.y(), eps);
    merge_unique(zs, p.z(), eps);
  };
  for (double h : {prism.d0, prism.d1})
  {
    for (const Point2d& uv : prism.Outer) add_uv(uv, h);
    for (const auto& hole : prism.Holes)
    {
      for (const Point2d& uv : hole) add_uv(uv, h);
    }
  }
}

}  // namespace

BooleanResult EvaluatePrismBoxBoolean(BooleanOp op, Model& model,
                                         const PlanarPrismSpec& prism,
                                         const BoxSpec& box, bool prism_is_a,
                                         const BooleanContext& ctx)
                                         {
  BooleanResult result;
  result.Mode = BooleanEvalMode::General;

  const double eps = std::max(ctx.fuzzy, 1e-12);
  std::vector<double> xs, ys, zs;
  collect_profile_coords(prism, xs, ys, zs, eps);
  merge_unique(xs, box.Min.x(), eps);
  merge_unique(xs, box.Max.x(), eps);
  merge_unique(ys, box.Min.y(), eps);
  merge_unique(ys, box.Max.y(), eps);
  merge_unique(zs, box.Min.z(), eps);
  merge_unique(zs, box.Max.z(), eps);
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

  auto in_prism = [&](const Point3d& mid)
  {
    return ClassifyPointInPrism(prism, mid, eps) != SolidClass::Out;
  };
  auto in_box = [&](const Point3d& mid)
  {
    return ClassifyPointInBox(box, mid, eps) != SolidClass::Out;
  };

  auto solid = [&](int i, int j, int k) -> bool {
    if (i < 0 || j < 0 || k < 0 || i >= nx || j >= ny || k >= nz) return false;
    const Point3d mid{(xs[i] + xs[i + 1]) * 0.5, (ys[j] + ys[j + 1]) * 0.5,
                      (zs[k] + zs[k + 1]) * 0.5};
    const bool pa = in_prism(mid);
    const bool ba = in_box(mid);
    const bool in_a = prism_is_a ? pa : ba;
    const bool in_b = prism_is_a ? ba : pa;
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

  // Fast path: filled AABB.
  Point3d rmin{1e300, 1e300, 1e300};
  Point3d rmax{-1e300, -1e300, -1e300};
  int solid_again = 0;
  for (int i = 0; i < nx; ++i)
  {
    for (int j = 0; j < ny; ++j)
  {
      for (int k = 0; k < nz; ++k)
  {
        if (!solid(i, j, k)) continue;
        ++solid_again;
        rmin = Point3d{std::min(rmin.x(), xs[i]), std::min(rmin.y(), ys[j]),
                       std::min(rmin.z(), zs[k])};
        rmax = Point3d{std::max(rmax.x(), xs[i + 1]),
                       std::max(rmax.y(), ys[j + 1]),
                       std::max(rmax.z(), zs[k + 1])};
      }
    }
  }
  int expect = 0;
  int got = 0;
  for (int i = 0; i < nx; ++i)
  {
    for (int j = 0; j < ny; ++j)
  {
      for (int k = 0; k < nz; ++k)
  {
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
  const bool is_filled_aabb =
      (expect > 0 && expect == got && solid_again == got);

  const std::string out_name =
      std::string("bool_prism_box_") + op_name(op);

  if (is_filled_aabb)
  {
    BoxSpec spec;
    spec.Min = rmin;
    spec.Max = rmax;
    spec.Tolerance = std::max(prism.Tolerance, box.Tolerance);
    spec.Name = out_name;
    result.OutputBody = MakeBox(model, spec);
    result.Mode = BooleanEvalMode::AnalyticPair;
    return result;
  }

  result.OutputBody = build_from_quads(model, quads, out_name,
                                 std::max(prism.Tolerance, box.Tolerance));
  if (!result.OutputBody)
  {
    result.Diagnostics =
        std::string("boolean ") + op_name(op) + ": failed to build manifold shell";
    BREP_WARN("{}", result.Diagnostics);
  }
  return result;
}

}  // namespace brep::boolean
