#include "brep/Builder.h"

#include "brep/Log.h"

#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace brep
{
namespace
{

constexpr int vid(int ix, int iy, int iz) noexcept
{
  return ix | (iy << 1) | (iz << 2);
}

Point3d corner(const BoxSpec& s, int ix, int iy, int iz)
{
  return {
      ix ? s.Max.x() : s.Min.x(),
      iy ? s.Max.y() : s.Min.y(),
      iz ? s.Max.z() : s.Min.z(),
  };
}

struct EdgeDef
{
  int a;
  int b;
  const char* Name;
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

struct FaceBuild
{
    const char* Name;
    Point3d Origin;
    Vector3d UAxis;
    Vector3d VAxis;
    std::array<int, 4> EdgeIdx;
    std::array<bool, 4> Forward;
    std::array<Point2d, 5> Uv;
};

}  // namespace

Body* MakeBox(Model& model, const BoxSpec& spec)
{
  if (!(spec.Max.x() > spec.Min.x() && spec.Max.y() > spec.Min.y() &&
        spec.Max.z() > spec.Min.z()))
  {
    BREP_ERROR("MakeBox: invalid extents min={} max={}", spec.Min, spec.Max);
    throw std::invalid_argument("MakeBox: max must be strictly greater than min");
  }

  BREP_INFO("MakeBox '{}' min={} max={} tol={:.3g}", spec.Name, spec.Min,
            spec.Max, spec.Tolerance);

  const double dx = spec.Max.x() - spec.Min.x();
  const double dy = spec.Max.y() - spec.Min.y();
  const double dz = spec.Max.z() - spec.Min.z();
  const double tol = spec.Tolerance;

  std::array<Vertex*, 8> V{};
  for (int iz = 0; iz < 2; ++iz)
  {
    for (int iy = 0; iy < 2; ++iy)
  {
      for (int ix = 0; ix < 2; ++ix)
  {
        const int i = vid(ix, iy, iz);
        Point* p = model.MakePoint(corner(spec, ix, iy, iz), "p" + std::to_string(i));
        V[i] = model.MakeVertex(p, tol, "v" + std::to_string(i));
      }
    }
  }

  std::array<Edge*, 12> E{};
  for (int i = 0; i < 12; ++i)
  {
    const EdgeDef& d = kEdges[i];
    LineCurve* curve = model.MakeLine(V[d.a]->Position(), V[d.b]->Position());
    E[i] = model.MakeEdge(curve, V[d.a], V[d.b], 0.0, curve->Length(), tol, d.Name);
  }

  const FaceBuild faces[6] = {
      // Loop must be CCW when viewed against the outward normal (-Z).
      {"f_zmin",
       {spec.Min.x(), spec.Min.y(), spec.Min.z()},
       {1, 0, 0},
       {0, -1, 0},
       {3, 2, 1, 0},
       {false, false, false, false},
       {Point2d{0, 0}, Point2d{0, -dy}, Point2d{dx, -dy}, Point2d{dx, 0},
        Point2d{0, 0}}},
      {"f_zmax",
       {spec.Min.x(), spec.Min.y(), spec.Max.z()},
       {1, 0, 0},
       {0, 1, 0},
       {4, 5, 6, 7},
       {true, true, true, true},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dy}, Point2d{0, dy},
        Point2d{0, 0}}},
      {"f_ymin",
       {spec.Min.x(), spec.Min.y(), spec.Min.z()},
       {1, 0, 0},
       {0, 0, 1},
       {0, 9, 4, 8},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
      {"f_ymax",
       {spec.Max.x(), spec.Max.y(), spec.Min.z()},
       {-1, 0, 0},
       {0, 0, 1},
       {2, 11, 6, 10},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dx, 0}, Point2d{dx, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
      {"f_xmin",
       {spec.Min.x(), spec.Min.y(), spec.Min.z()},
       {0, 0, 1},
       {0, 1, 0},
       {8, 7, 11, 3},
       {true, false, false, true},
       {Point2d{0, 0}, Point2d{dz, 0}, Point2d{dz, dy}, Point2d{0, dy},
        Point2d{0, 0}}},
      {"f_xmax",
       {spec.Max.x(), spec.Min.y(), spec.Min.z()},
       {0, 1, 0},
       {0, 0, 1},
       {1, 10, 5, 9},
       {true, true, false, false},
       {Point2d{0, 0}, Point2d{dy, 0}, Point2d{dy, dz}, Point2d{0, dz},
        Point2d{0, 0}}},
  };

  Body* body = model.MakeBody(BodyType::Solid, spec.Name);
  Shell* shell = model.MakeShell(true, spec.Name + "_shell");
  body->Shells.push_back(shell);

  std::array<std::vector<CoEdge*>, 12> by_edge{};

  for (const FaceBuild& fd : faces)
  {
    PlaneSurface* surf = model.MakePlane(fd.Origin, fd.UAxis, fd.VAxis, fd.Name);
    Face* face = model.MakeFace(surf, Orientation::Forward, fd.Name);
    shell->Faces.push_back(face);
    Loop* loop = model.MakeLoop(face, LoopType::Outer, std::string(fd.Name) + "_outer");

    std::array<CoEdge*, 4> ces{};
    for (int k = 0; k < 4; ++k)
    {
      const int ei = fd.EdgeIdx[k];
      const Orientation sense =
          fd.Forward[k] ? Orientation::Forward : Orientation::Reversed;
      Curve2d* pc = model.MakeLine2d(fd.Uv[k], fd.Uv[k + 1]);
      ces[k] = model.MakeCoedge(
          E[ei], sense, pc, std::string(fd.Name) + "_ce" + std::to_string(k));
      by_edge[static_cast<std::size_t>(ei)].push_back(ces[k]);
    }
    Model::LinkLoop(loop, ces);
  }

  for (int i = 0; i < 12; ++i)
  {
    if (by_edge[static_cast<std::size_t>(i)].size() != 2)
  {
      BREP_ERROR("MakeBox: edge[{}] radial degree={}", i,
                 by_edge[static_cast<std::size_t>(i)].size());
      throw std::runtime_error(
          "MakeBox: each edge must be shared by exactly two faces");
    }
    Model::PairPartners(by_edge[static_cast<std::size_t>(i)][0],
                         by_edge[static_cast<std::size_t>(i)][1]);
  }

  BREP_INFO("MakeBox '{}' done: 8 verts, 12 edges, 6 faces", spec.Name);
  return body;
}

Body* MakeSphere(Model& model, const SphereSpec& spec)
{
  if (!(spec.Radius > 0.0))
{
    BREP_ERROR("MakeSphere: invalid radius={}", spec.Radius);
    throw std::invalid_argument("MakeSphere: radius must be positive");
  }

  const Point3d& c = spec.Center;
  const double r = spec.Radius;
  const double tol = spec.Tolerance;
  // slices/stacks are retained for API compatibility / debug only; topology
  // is analytic (dual poles + meridional seam + one spherical face).
  BREP_INFO("MakeSphere '{}' center={} r={:.6g} (analytic; slices={} stacks={} unused)",
            spec.Name, c, r, spec.Slices, spec.Stacks);

  const Point3d south_xyz{c.x(), c.y() - r, c.z()};
  const Point3d north_xyz{c.x(), c.y() + r, c.z()};
  Vertex* v_s =
      model.MakeVertex(model.MakePoint(south_xyz, spec.Name + "_ps"), tol,
                        spec.Name + "_vs");
  Vertex* v_n =
      model.MakeVertex(model.MakePoint(north_xyz, spec.Name + "_pn"), tol,
                        spec.Name + "_vn");

  // Meridian seam at SphereSurface u=0: half great circle in the XY plane
  // (circle normal +Z). CircleCurve(+Z): eval(π)=south, eval(2π)=north via +X.
  CircleCurve* seam_curve =
      model.MakeCircle(c, Vector3d{0, 0, 1}, r, spec.Name + "_seam_crv");
  Edge* e_seam =
      model.MakeEdge(seam_curve, v_s, v_n, std::numbers::pi,
                      2.0 * std::numbers::pi, tol, spec.Name + "_seam");

  SphereSurface* surf =
      model.MakeSphereSurface(c, r, spec.Name + "_surf");
  Body* body = model.MakeBody(BodyType::Solid, spec.Name);
  Shell* shell = model.MakeShell(true, spec.Name + "_shell");
  body->Shells.push_back(shell);

  Face* face =
      model.MakeFace(surf, Orientation::Forward, spec.Name + "_face");
  shell->Faces.push_back(face);
  Loop* loop =
      model.MakeLoop(face, LoopType::Outer, spec.Name + "_outer");

  // UV outer: up u=0 (south→north) then down u=2π (north→south).
  Curve2d* pc_fwd = model.MakeLine2d(
      Point2d{0.0, -0.5 * std::numbers::pi},
      Point2d{0.0, 0.5 * std::numbers::pi});
  Curve2d* pc_rev = model.MakeLine2d(
      Point2d{2.0 * std::numbers::pi, 0.5 * std::numbers::pi},
      Point2d{2.0 * std::numbers::pi, -0.5 * std::numbers::pi});
  CoEdge* ce_fwd =
      model.MakeCoedge(e_seam, Orientation::Forward, pc_fwd,
                        spec.Name + "_ce_fwd");
  CoEdge* ce_rev =
      model.MakeCoedge(e_seam, Orientation::Reversed, pc_rev,
                        spec.Name + "_ce_rev");
  Model::LinkLoop(loop, std::array<CoEdge*, 2>{ce_fwd, ce_rev});
  Model::PairPartners(ce_fwd, ce_rev);

  BREP_INFO("MakeSphere '{}' done: 2 verts, 1 seam, 1 face", spec.Name);
  return body;
}

}  // namespace brep
