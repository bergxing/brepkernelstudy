#include "brep/bool/SphereSphereBoolean.h"

#include "brep/bool/IntersectSphereSphere.h"
#include "brep/Geometry.h"
#include "brep/Log.h"
#include "brep/Validate.h"

#include <array>
#include <cmath>
#include <numbers>
#include <string>

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

[[nodiscard]] Orientation sense_along(Edge* edge, Vertex* from, Vertex* to)
{
  if (edge->V0 == from && edge->V1 == to) return Orientation::Forward;
  if (edge->V0 == to && edge->V1 == from) return Orientation::Reversed;
  const double d0 = (edge->V0->Position() - from->Position()).squaredNorm();
  const double d1 = (edge->V1->Position() - from->Position()).squaredNorm();
  return d0 <= d1 ? Orientation::Forward : Orientation::Reversed;
}

Body* build_two_cap_union(Model& model, const SphereSpec& sa,
                          const SphereSpec& sb, const Point3d& circ_c,
                          const Vector3d& circ_n, double circ_r, double tol,
                          const std::string& name)
                          {
  const Vector3d n = circ_n.normalized();
  const Vector3d ref =
      std::abs(n.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
  const Vector3d x = n.cross(ref).normalized();

  Vertex* v0 =
      model.MakeVertex(model.MakePoint(circ_c + x * circ_r), tol, name + "_v0");
  Vertex* v1 =
      model.MakeVertex(model.MakePoint(circ_c - x * circ_r), tol, name + "_v1");

  CircleCurve* curve = model.MakeCircle(circ_c, n, circ_r, name + "_circ");
  const double ta = circle_ParamAt(*curve, v0->Position());
  // Antipodal halves: v0 at ta, v1 at ta+π (v0/v1 are opposite on the circle).
  Edge* e0 = model.MakeEdge(curve, v0, v1, ta, ta + std::numbers::pi, tol,
                             name + "_e0");
  Edge* e1 = model.MakeEdge(curve, v1, v0, ta + std::numbers::pi,
                             ta + 2.0 * std::numbers::pi, tol, name + "_e1");

  Body* body = model.MakeBody(BodyType::Solid, name);
  Shell* shell = model.MakeShell(true, name + "_shell");
  body->Shells.push_back(shell);

  auto add_cap = [&](const SphereSpec& spec, bool reverse_loop,
                     const std::string& fname)
  {
    SphereSurface* surf =
        model.MakeSphereSurface(spec.Center, spec.Radius, fname);
    Face* face = model.MakeFace(surf, Orientation::Forward, fname);
    shell->Faces.push_back(face);
    Loop* loop = model.MakeLoop(face, LoopType::Outer, fname + "_outer");
    if (reverse_loop)
    {
      CoEdge* c0 = model.MakeCoedge(e1, sense_along(e1, v0, v1));
      CoEdge* c1 = model.MakeCoedge(e0, sense_along(e0, v1, v0));
      Model::LinkLoop(loop, std::array<CoEdge*, 2>{c0, c1});
    }
    else
    {
      CoEdge* c0 = model.MakeCoedge(e0, sense_along(e0, v0, v1));
      CoEdge* c1 = model.MakeCoedge(e1, sense_along(e1, v1, v0));
      Model::LinkLoop(loop, std::array<CoEdge*, 2>{c0, c1});
    }
  };

  add_cap(sa, false, name + "_fa");
  add_cap(sb, true, name + "_fb");

  for (Edge* e : {e0, e1})
  {
    if (e->Radial.size() != 2)
  {
      BREP_ERROR("sphere union: edge '{}' radial={}", e->Name, e->Radial.size());
      return nullptr;
    }
    Model::PairPartners(e->Radial[0], e->Radial[1]);
  }

  const auto report = ValidateBody(*body);
  if (!report.Ok())
  {
    for (const auto& issue : report.Issues)
  {
      BREP_WARN("sphere union validate [{}] {}", issue.Where, issue.Message);
    }
    return nullptr;
  }
  return body;
}

Body* clone_sphere_body(Model& model, const SphereSpec& spec,
                        const std::string& name)
{
  SphereSpec copy = spec;
  copy.Name = name;
  return MakeSphere(model, copy);
}

}  // namespace

BooleanResult EvaluateSphereSphereBoolean(BooleanOp op, Model& model,
                                             const SphereSpec& a,
                                             const SphereSpec& b,
                                             const BooleanContext& ctx)
                                             {
  BooleanResult result;
  result.Mode = BooleanEvalMode::AnalyticPair;

  if (op != BooleanOp::Union)
  {
    result.Diagnostics =
        std::string("boolean ") + op_name(op) +
        ": sphere–sphere currently supports Union only (T3.8)";
    BREP_WARN("{}", result.Diagnostics);
    return result;
  }

  const auto ix =
      IntersectSphereSphere(a.Center, a.Radius, b.Center, b.Radius, ctx);
  const double tol = std::max(a.Tolerance, b.Tolerance);
  const std::string name = "bool_sphere_sphere_Union";

  switch (ix.status)
  {
    case SphereSphereStatus::Circle: {
      result.OutputBody = build_two_cap_union(model, a, b, ix.Center, ix.Normal,
                                       ix.Radius, tol, name);
      if (!result.OutputBody)
      {
        result.Diagnostics =
            "boolean Union: failed to build intersecting Sphere∪Sphere shell";
      }
      return result;
    }
    case SphereSphereStatus::Contained: {
      const SphereSpec& larger = a.Radius >= b.Radius ? a : b;
      result.OutputBody = clone_sphere_body(model, larger, name);
      if (!result.OutputBody)
      {
        result.Diagnostics = "boolean Union: failed to clone containing sphere";
      }
      return result;
    }
    case SphereSphereStatus::Coincident: {
      result.OutputBody = clone_sphere_body(model, a, name);
      if (!result.OutputBody)
      {
        result.Diagnostics = "boolean Union: failed to clone coincident sphere";
      }
      return result;
    }
    case SphereSphereStatus::Point: {
      result.Diagnostics =
          "boolean Union: sphere–sphere tangency (point) not built as a single "
          "manifold yet";
      BREP_WARN("{}", result.Diagnostics);
      return result;
    }
    case SphereSphereStatus::Separate:
    default:
      result.Diagnostics =
          "boolean Union: sphere–sphere separate �?disconnected multi-body "
          "union not implemented";
      BREP_WARN("{}", result.Diagnostics);
      return result;
  }
}

}  // namespace brep::boolean
