#include "brep/bool/SphereBoxBoolean.h"

#include "brep/Geometry.h"
#include "brep/Log.h"
#include "brep/Validate.h"

#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

namespace brep::boolean
{
namespace
{

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

[[nodiscard]] double norm_angle(double t)
{
  const double twopi = 2.0 * std::numbers::pi;
  t = std::fmod(t, twopi);
  if (t < 0.0) t += twopi;
  return t;
}

[[nodiscard]] double circle_ParamAt(const CircleCurve& curve, const Point3d& p)
{
  const Point3d p0 = curve.Eval(0.0);
  const Point3d p90 = curve.Eval(0.5 * std::numbers::pi);
  const Vector3d x_axis = (p0 - curve.Center()).normalized();
  const Vector3d y_axis = (p90 - curve.Center()).normalized();
  const Vector3d d = p - curve.Center();
  return norm_angle(std::atan2(d.dot(y_axis), d.dot(x_axis)));
}

Edge* make_arc_edge(Model& model, CircleCurve* curve, Vertex* a, Vertex* b,
                    double tol, const std::string& name)
{
  double t0 = circle_ParamAt(*curve, a->Position());
  double t1 = circle_ParamAt(*curve, b->Position());
  double forward = norm_angle(t1 - t0);
  if (forward > std::numbers::pi)
  {
    std::swap(a, b);
    std::swap(t0, t1);
    forward = norm_angle(t1 - t0);
  }
  if (t1 < t0) t1 += 2.0 * std::numbers::pi;
  return model.MakeEdge(curve, a, b, t0, t1, tol, name);
}

[[nodiscard]] Orientation sense_along(Edge* edge, Vertex* from, Vertex* to)
{
  if (edge->V0 == from && edge->V1 == to) return Orientation::Forward;
  if (edge->V0 == to && edge->V1 == from) return Orientation::Reversed;
  const double d0 = (edge->V0->Position() - from->Position()).squaredNorm();
  const double d1 = (edge->V1->Position() - from->Position()).squaredNorm();
  return d0 <= d1 ? Orientation::Forward : Orientation::Reversed;
}

struct CornerOctant
{
  Point3d c{};
  double sx{1.0};
  double sy{1.0};
  double sz{1.0};
};

[[nodiscard]] bool point_in_box(const BoxSpec& box, const Point3d& p,
                                double eps)
{
  return p.x() >= box.Min.x() - eps && p.x() <= box.Max.x() + eps &&
         p.y() >= box.Min.y() - eps && p.y() <= box.Max.y() + eps &&
         p.z() >= box.Min.z() - eps && p.z() <= box.Max.z() + eps;
}

[[nodiscard]] std::optional<CornerOctant> detect_corner_pose(
    const SphereSpec& sphere, const BoxSpec& box, double eps)
{
  const Point3d& c = sphere.Center;
  const Point3d corners[8] = {
      {box.Min.x(), box.Min.y(), box.Min.z()},
      {box.Max.x(), box.Min.y(), box.Min.z()},
      {box.Min.x(), box.Max.y(), box.Min.z()},
      {box.Max.x(), box.Max.y(), box.Min.z()},
      {box.Min.x(), box.Min.y(), box.Max.z()},
      {box.Max.x(), box.Min.y(), box.Max.z()},
      {box.Min.x(), box.Max.y(), box.Max.z()},
      {box.Max.x(), box.Max.y(), box.Max.z()},
  };

  for (const Point3d& corner : corners)
  {
    if (std::abs(c.x() - corner.x()) > eps ||
        std::abs(c.y() - corner.y()) > eps ||
        std::abs(c.z() - corner.z()) > eps)
        {
      continue;
    }
    CornerOctant pose;
    pose.c = c;
    pose.sx = (std::abs(corner.x() - box.Min.x()) <= eps) ? 1.0 : -1.0;
    pose.sy = (std::abs(corner.y() - box.Min.y()) <= eps) ? 1.0 : -1.0;
    pose.sz = (std::abs(corner.z() - box.Min.z()) <= eps) ? 1.0 : -1.0;
    return pose;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<CornerOctant> detect_corner_octant(
    const SphereSpec& sphere, const BoxSpec& box, double eps)
{
  const auto pose = detect_corner_pose(sphere, box, eps);
  if (!pose) return std::nullopt;

  const double r = sphere.Radius;
  const Point3d& c = pose->c;
  const Point3d ax{c.x() + pose->sx * r, c.y(), c.z()};
  const Point3d ay{c.x(), c.y() + pose->sy * r, c.z()};
  const Point3d az{c.x(), c.y(), c.z() + pose->sz * r};
  const Point3d far{c.x() + pose->sx * r, c.y() + pose->sy * r,
                    c.z() + pose->sz * r};
  if (point_in_box(box, ax, eps) && point_in_box(box, ay, eps) &&
      point_in_box(box, az, eps) && point_in_box(box, far, eps))
  {
    return pose;
  }
  return std::nullopt;
}

/// `seven_eighths==false` → inward ⅛ ball; true → sphere minus that octant.
Body* build_axis_octant_ball(Model& model, const CornerOctant& pose, double r,
                             double tol, const std::string& name,
                             bool seven_eighths)
                             {
  const Point3d& c = pose.c;
  const Point3d ax{c.x() + pose.sx * r, c.y(), c.z()};
  const Point3d ay{c.x(), c.y() + pose.sy * r, c.z()};
  const Point3d az{c.x(), c.y(), c.z() + pose.sz * r};

  Vertex* vo = model.MakeVertex(model.MakePoint(c), tol, name + "_o");
  Vertex* vx = model.MakeVertex(model.MakePoint(ax), tol, name + "_ax");
  Vertex* vy = model.MakeVertex(model.MakePoint(ay), tol, name + "_ay");
  Vertex* vz = model.MakeVertex(model.MakePoint(az), tol, name + "_az");

  auto make_line_edge = [&](Vertex* a, Vertex* b, const std::string& en)
  {
    LineCurve* curve =
        model.MakeLine(a->Position(), b->Position(), en + "_crv");
    return model.MakeEdge(curve, a, b, 0.0, r, tol, en);
  };

  Edge* e_ox = make_line_edge(vo, vx, name + "_eox");
  Edge* e_oy = make_line_edge(vo, vy, name + "_eoy");
  Edge* e_oz = make_line_edge(vo, vz, name + "_eoz");

  CircleCurve* c_xy =
      model.MakeCircle(c, Vector3d{0, 0, pose.sx * pose.sy}, r, name + "_cxy");
  CircleCurve* c_yz =
      model.MakeCircle(c, Vector3d{pose.sy * pose.sz, 0, 0}, r, name + "_cyz");
  CircleCurve* c_zx =
      model.MakeCircle(c, Vector3d{0, pose.sz * pose.sx, 0}, r, name + "_czx");

  Edge* e_xy = make_arc_edge(model, c_xy, vx, vy, tol, name + "_exy");
  Edge* e_yz = make_arc_edge(model, c_yz, vy, vz, tol, name + "_eyz");
  Edge* e_zx = make_arc_edge(model, c_zx, vz, vx, tol, name + "_ezx");

  Body* body = model.MakeBody(BodyType::Solid, name);
  Shell* shell = model.MakeShell(true, name + "_shell");
  body->Shells.push_back(shell);

  // u×v = sx·sy·sz along the local +Z/+X/+Y axis respectively. Outward from the
  // inward octant is −sx/−sy/−sz on each plane, so face sense alternates with
  // octant parity (sx·sy·sz).
  const double oct_parity = pose.sx * pose.sy * pose.sz;
  const Orientation plane_sense =
      (oct_parity > 0.0) == seven_eighths ? Orientation::Forward
                                          : Orientation::Reversed;
  const bool plane_loop_reversed = (oct_parity > 0.0) == seven_eighths;

  auto add_plane_face = [&](Vector3d u, Vector3d v, Edge* e0, Vertex* a0,
                            Vertex* b0, Edge* e1, Vertex* a1, Vertex* b1,
                            Edge* e2, Vertex* a2, Vertex* b2,
                            const std::string& fname)
                            {
    PlaneSurface* surf = model.MakePlane(c, u, v, fname);
    Face* face = model.MakeFace(surf, plane_sense, fname);
    shell->Faces.push_back(face);
    Loop* loop = model.MakeLoop(face, LoopType::Outer, fname + "_outer");
    if (plane_loop_reversed)
    {
      CoEdge* c0 = model.MakeCoedge(e2, sense_along(e2, b2, a2));
      CoEdge* c1 = model.MakeCoedge(e1, sense_along(e1, b1, a1));
      CoEdge* c2 = model.MakeCoedge(e0, sense_along(e0, b0, a0));
      Model::LinkLoop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    }
    else
    {
      CoEdge* c0 = model.MakeCoedge(e0, sense_along(e0, a0, b0));
      CoEdge* c1 = model.MakeCoedge(e1, sense_along(e1, a1, b1));
      CoEdge* c2 = model.MakeCoedge(e2, sense_along(e2, a2, b2));
      Model::LinkLoop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    }
  };

  // Planes through C spanning inward axes (u = sx·X̂, v = sy·Ŷ, etc.).
  const Vector3d ex{pose.sx, 0, 0};
  const Vector3d ey{0, pose.sy, 0};
  const Vector3d ez{0, 0, pose.sz};

  add_plane_face(ex, ey, e_ox, vo, vx, e_xy, vx, vy, e_oy, vy, vo, name + "_fz");
  add_plane_face(ey, ez, e_oy, vo, vy, e_yz, vy, vz, e_oz, vz, vo, name + "_fx");
  add_plane_face(ez, ex, e_oz, vo, vz, e_zx, vz, vx, e_ox, vx, vo, name + "_fy");

  {
    SphereSurface* surf = model.MakeSphereSurface(c, r, name + "_fs");
    Face* face = model.MakeFace(surf, Orientation::Forward, name + "_fs");
    shell->Faces.push_back(face);
    Loop* loop = model.MakeLoop(face, LoopType::Outer, name + "_fs_outer");
    if (seven_eighths)
    {
      CoEdge* c0 = model.MakeCoedge(e_xy, sense_along(e_xy, vx, vy));
      CoEdge* c1 = model.MakeCoedge(e_yz, sense_along(e_yz, vy, vz));
      CoEdge* c2 = model.MakeCoedge(e_zx, sense_along(e_zx, vz, vx));
      Model::LinkLoop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    }
    else
    {
      CoEdge* c0 = model.MakeCoedge(e_zx, sense_along(e_zx, vx, vz));
      CoEdge* c1 = model.MakeCoedge(e_yz, sense_along(e_yz, vz, vy));
      CoEdge* c2 = model.MakeCoedge(e_xy, sense_along(e_xy, vy, vx));
      Model::LinkLoop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
    }
  }

  for (Edge* e : {e_ox, e_oy, e_oz, e_xy, e_yz, e_zx})
  {
    if (e->Radial.size() != 2)
  {
      BREP_ERROR("octant ball: edge '{}' radial={}", e->Name, e->Radial.size());
      return nullptr;
    }
    Model::PairPartners(e->Radial[0], e->Radial[1]);
  }

  const auto report = ValidateBody(*body);
  if (!report.Ok())
  {
    for (const auto& issue : report.Issues)
  {
      BREP_WARN("octant ball validate [{}] {}", issue.Where, issue.Message);
    }
    return nullptr;
  }
  return body;
}

[[nodiscard]] Face* find_plane_face(Body& body, const Point3d& origin,
                                    const Vector3d& outward, double eps)
{
  const Vector3d n = outward.normalized();
  for (Shell* shell : body.Shells)
  {
    if (!shell) continue;
    for (Face* face : shell->Faces)
    {
      if (!face || !face->Surface ||
          face->Surface->Kind() != SurfaceKind::Plane)
      {
        continue;
      }
      const auto& pl = static_cast<const PlaneSurface&>(*face->Surface);
      Vector3d fn = pl.Normal(0, 0);
      if (face->Sense == Orientation::Reversed) fn = -fn;
      if (fn.dot(n) < 1.0 - 1e-6) continue;
      const Vector3d d = origin - pl.Origin();
      if (std::abs(d.dot(fn)) > eps) continue;
      return face;
    }
  }
  return nullptr;
}

/// Box ∪ sphere when sphere center is at a box corner and the inward octant of
/// the ball lies inside the box. Topology: box + spherical ⅞ face + 3 Inner
/// quarter-circle holes on the corner faces.
Body* build_sphere_box_union(Model& model, const CornerOctant& pose,
                             const BoxSpec& box, double r, double tol,
                             const std::string& name)
                             {
  BoxSpec box_copy = box;
  box_copy.Name = name + "_box";
  Body* body = MakeBox(model, box_copy);
  if (!body || body->Shells.empty()) return nullptr;
  Shell* shell = body->Shells.front();
  body->Name = name;

  const Point3d& c = pose.c;
  Vertex* vo = nullptr;
  for (Shell* sh : body->Shells)
  {
    for (Face* f : sh->Faces)
  {
      for (Loop* lp : f->Loops)
  {
        if (!lp) continue;
        lp->ForEachCoedge([&](const CoEdge& ce)
        {
          if (Vertex* v = ce.From())
        {
            const Point3d& p = v->Position();
            if (std::abs(p.x() - c.x()) <= tol &&
                std::abs(p.y() - c.y()) <= tol &&
                std::abs(p.z() - c.z()) <= tol)
                {
              vo = v;
            }
          }
        });
      }
    }
  }
  if (!vo)
  {
    BREP_ERROR("sphere-box union: corner vertex not found");
    return nullptr;
  }

  const Point3d ax{c.x() + pose.sx * r, c.y(), c.z()};
  const Point3d ay{c.x(), c.y() + pose.sy * r, c.z()};
  const Point3d az{c.x(), c.y(), c.z() + pose.sz * r};
  Vertex* vx = model.MakeVertex(model.MakePoint(ax), tol, name + "_ax");
  Vertex* vy = model.MakeVertex(model.MakePoint(ay), tol, name + "_ay");
  Vertex* vz = model.MakeVertex(model.MakePoint(az), tol, name + "_az");

  auto make_line_edge = [&](Vertex* a, Vertex* b, const std::string& en)
  {
    LineCurve* curve =
        model.MakeLine(a->Position(), b->Position(), en + "_crv");
    return model.MakeEdge(curve, a, b, 0.0, r, tol, en);
  };
  Edge* e_ox = make_line_edge(vo, vx, name + "_eox");
  Edge* e_oy = make_line_edge(vo, vy, name + "_eoy");
  Edge* e_oz = make_line_edge(vo, vz, name + "_eoz");

  CircleCurve* c_xy =
      model.MakeCircle(c, Vector3d{0, 0, pose.sx * pose.sy}, r, name + "_cxy");
  CircleCurve* c_yz =
      model.MakeCircle(c, Vector3d{pose.sy * pose.sz, 0, 0}, r, name + "_cyz");
  CircleCurve* c_zx =
      model.MakeCircle(c, Vector3d{0, pose.sz * pose.sx, 0}, r, name + "_czx");
  Edge* e_xy = make_arc_edge(model, c_xy, vx, vy, tol, name + "_exy");
  Edge* e_yz = make_arc_edge(model, c_yz, vy, vz, tol, name + "_eyz");
  Edge* e_zx = make_arc_edge(model, c_zx, vz, vx, tol, name + "_ezx");

  // Outward normals of the three box faces at the corner (point out of box).
  const Vector3d out_x{-pose.sx, 0, 0};
  const Vector3d out_y{0, -pose.sy, 0};
  const Vector3d out_z{0, 0, -pose.sz};
  Face* fx = find_plane_face(*body, c, out_x, tol);
  Face* fy = find_plane_face(*body, c, out_y, tol);
  Face* fz = find_plane_face(*body, c, out_z, tol);
  if (!fx || !fy || !fz)
  {
    BREP_ERROR("sphere-box union: corner faces not found");
    return nullptr;
  }

  auto add_inner = [&](Face* face, Edge* e0, Vertex* a0, Vertex* b0, Edge* e1,
                       Vertex* a1, Vertex* b1, Edge* e2, Vertex* a2, Vertex* b2,
                       const std::string& lname)
                       {
    Loop* loop = model.MakeLoop(face, LoopType::Inner, lname);
    // Same reverse order as ⅞ planar faces: hole leaves material outside.
    CoEdge* c0 = model.MakeCoedge(e2, sense_along(e2, b2, a2));
    CoEdge* c1 = model.MakeCoedge(e1, sense_along(e1, b1, a1));
    CoEdge* c2 = model.MakeCoedge(e0, sense_along(e0, b0, a0));
    Model::LinkLoop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
  };

  // Face z (out_z): hole O→Ay→Ax→O (reverse of O→Ax→Ay→O).
  add_inner(fz, e_ox, vo, vx, e_xy, vx, vy, e_oy, vy, vo, name + "_fz_inner");
  // Face x: O→Az→Ay→O
  add_inner(fx, e_oy, vo, vy, e_yz, vy, vz, e_oz, vz, vo, name + "_fx_inner");
  // Face y: O→Ax→Az→O
  add_inner(fy, e_oz, vo, vz, e_zx, vz, vx, e_ox, vx, vo, name + "_fy_inner");

  {
    SphereSurface* surf = model.MakeSphereSurface(c, r, name + "_fs");
    Face* face = model.MakeFace(surf, Orientation::Forward, name + "_fs");
    shell->Faces.push_back(face);
    Loop* loop = model.MakeLoop(face, LoopType::Outer, name + "_fs_outer");
    // ⅞-style winding (exterior spherical patch).
    CoEdge* c0 = model.MakeCoedge(e_xy, sense_along(e_xy, vx, vy));
    CoEdge* c1 = model.MakeCoedge(e_yz, sense_along(e_yz, vy, vz));
    CoEdge* c2 = model.MakeCoedge(e_zx, sense_along(e_zx, vz, vx));
    Model::LinkLoop(loop, std::array<CoEdge*, 3>{c0, c1, c2});
  }

  for (Edge* e : {e_ox, e_oy, e_oz, e_xy, e_yz, e_zx})
  {
    if (e->Radial.size() != 2)
  {
      BREP_ERROR("sphere-box union: edge '{}' radial={}", e->Name,
                 e->Radial.size());
      return nullptr;
    }
    Model::PairPartners(e->Radial[0], e->Radial[1]);
  }

  const auto report = ValidateBody(*body);
  if (!report.Ok())
  {
    for (const auto& issue : report.Issues)
  {
      BREP_WARN("sphere-box union validate [{}] {}", issue.Where, issue.Message);
    }
    return nullptr;
  }
  return body;
}

[[nodiscard]] bool sphere_inside_box(const SphereSpec& s, const BoxSpec& b,
                                     double eps)
{
  return s.Center.x() - s.Radius >= b.Min.x() - eps &&
         s.Center.x() + s.Radius <= b.Max.x() + eps &&
         s.Center.y() - s.Radius >= b.Min.y() - eps &&
         s.Center.y() + s.Radius <= b.Max.y() + eps &&
         s.Center.z() - s.Radius >= b.Min.z() - eps &&
         s.Center.z() + s.Radius <= b.Max.z() + eps;
}

[[nodiscard]] bool box_inside_sphere(const BoxSpec& b, const SphereSpec& s,
                                     double eps)
{
  const Point3d corners[8] = {
      {b.Min.x(), b.Min.y(), b.Min.z()}, {b.Max.x(), b.Min.y(), b.Min.z()},
      {b.Min.x(), b.Max.y(), b.Min.z()}, {b.Max.x(), b.Max.y(), b.Min.z()},
      {b.Min.x(), b.Min.y(), b.Max.z()}, {b.Max.x(), b.Min.y(), b.Max.z()},
      {b.Min.x(), b.Max.y(), b.Max.z()}, {b.Max.x(), b.Max.y(), b.Max.z()},
  };
  for (const Point3d& p : corners)
  {
    if ((p - s.Center).norm() > s.Radius + eps) return false;
  }
  return true;
}

}  // namespace

BooleanResult EvaluateSphereBoxBoolean(BooleanOp op, Model& model,
                                          const SphereSpec& sphere,
                                          const BoxSpec& box, bool sphere_is_a,
                                          const BooleanContext& ctx)
                                          {
  BooleanResult result;
  result.Mode = BooleanEvalMode::AnalyticPair;
  const double eps = std::max(ctx.fuzzy, 1e-9);
  const double tol = std::max(sphere.Tolerance, box.Tolerance);
  const auto strict_pose = detect_corner_octant(sphere, box, eps);
  const auto corner_pose = strict_pose ? strict_pose
                                       : detect_corner_pose(sphere, box, eps);

  if (op == BooleanOp::Intersect)
  {
    if (!strict_pose)
  {
      result.Diagnostics =
          "boolean Intersect: sphere–box requires sphere center at a box "
          "corner with the inward octant of the ball inside the box";
      BREP_WARN("{}", result.Diagnostics);
      return result;
    }
    result.OutputBody = build_axis_octant_ball(
        model, *strict_pose, sphere.Radius, tol,
        std::string("bool_sphere_box_") + op_name(op), /*seven_eighths=*/false);
    if (!result.OutputBody)
    {
      result.Diagnostics =
          std::string("boolean ") + op_name(op) + ": failed to build ⅛-ball";
    }
    return result;
  }

  if (op == BooleanOp::Subtract && sphere_is_a)
  {
    if (!corner_pose)
  {
      result.Diagnostics =
          "boolean Subtract: Sphere−Box requires sphere center at a box "
          "corner (⅞-ball)";
      BREP_WARN("{}", result.Diagnostics);
      return result;
    }
    result.OutputBody = build_axis_octant_ball(
        model, *corner_pose, sphere.Radius, tol,
        std::string("bool_sphere_box_") + op_name(op), /*seven_eighths=*/true);
    if (!result.OutputBody)
    {
      result.Diagnostics =
          "boolean Subtract: failed to build Sphere−Box (⅞-ball)";
    }
    return result;
  }

  if (op == BooleanOp::Subtract && !sphere_is_a)
  {
    result.Diagnostics =
        "boolean Subtract: Box−Sphere is not implemented yet (T3.7 covers "
        "Sphere−Box only)";
    BREP_WARN("{}", result.Diagnostics);
    return result;
  }

  if (op == BooleanOp::Union)
  {
    if (sphere_inside_box(sphere, box, eps))
  {
      BoxSpec copy = box;
      copy.Name = "bool_sphere_box_Union";
      result.OutputBody = MakeBox(model, copy);
      return result;
    }
    if (box_inside_sphere(box, sphere, eps))
    {
      SphereSpec copy = sphere;
      copy.Name = "bool_sphere_box_Union";
      result.OutputBody = MakeSphere(model, copy);
      return result;
    }
    if (!corner_pose)
    {
      result.Diagnostics =
          "boolean Union: sphere–box requires containment or sphere center at "
          "a box corner";
      BREP_WARN("{}", result.Diagnostics);
      return result;
    }
    result.OutputBody = build_sphere_box_union(model, *corner_pose, box, sphere.Radius,
                                        tol,
                                        "bool_sphere_box_Union");
    if (!result.OutputBody)
    {
      result.Diagnostics = "boolean Union: failed to build Sphere∪Box shell";
    }
    return result;
  }

  result.Diagnostics =
      std::string("boolean ") + op_name(op) +
      ": sphere–box unsupported operation";
  BREP_WARN("{}", result.Diagnostics);
  return result;
}

}  // namespace brep::boolean
