#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/Geometry.h"
#include "brep/Mesh.h"
#include "brep/ops/Profile.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>

namespace brep
{
namespace
{

[[nodiscard]] bool point_in_triangle(const Point3d& p, const Point3d& a,
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

[[nodiscard]] double triangle_area(const Point3d& a, const Point3d& b,
                                   const Point3d& c)
{
  return 0.5 * (b - a).cross(c - a).norm();
}

TEST(TessellateInner, HoleCenterNotCovered)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "plate_hole";
  spec.distance = 1.0;
  spec.plane = Plane::xy();
  spec.profile.outer = {
      Point2d{0, 0},
      Point2d{4, 0},
      Point2d{4, 4},
      Point2d{0, 4},
  };
  spec.profile.holes.push_back({
      Point2d{1, 1},
      Point2d{3, 1},
      Point2d{3, 3},
      Point2d{1, 3},
  });

  Body* body = ops::extrude(model, spec);
  ASSERT_NE(body, nullptr);

  const TriangleMesh mesh = tessellate_body(*body);
  ASSERT_FALSE(mesh.indices.empty());

  // Hole center on the top cap (z = 1).
  const Point3d hole_center{2, 2, 1};
  bool covered = false;
  double top_area = 0.0;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
  {
    const Point3d& a = mesh.vertices[mesh.indices[i]].position;
    const Point3d& b = mesh.vertices[mesh.indices[i + 1]].position;
    const Point3d& c = mesh.vertices[mesh.indices[i + 2]].position;
    // Only consider triangles near the top plane z≈1.
    const double zavg = (a.z() + b.z() + c.z()) / 3.0;
    if (std::abs(zavg - 1.0) > 1e-6) continue;
    top_area += triangle_area(a, b, c);
    if (point_in_triangle(hole_center, a, b, c)) covered = true;
  }

  EXPECT_FALSE(covered) << "hole center should not lie in any top triangle";
  // Outer 16 − hole 4 = 12
  EXPECT_NEAR(top_area, 12.0, 0.25);
}

/// Sheet face: outer unit square; inner triangle shares corner Vertex* at (0,0).
Body* make_corner_touching_hole_sheet(Model& model)
{
  constexpr double tol = 1e-7;
  PlaneSurface* surf =
      model.make_plane(Point3d{0, 0, 0}, Vector3d{1, 0, 0}, Vector3d{0, 1, 0},
                       "corner_hole_surf");
  Body* body = model.make_body(BodyType::Sheet, "corner_hole");
  Shell* shell = model.make_shell(false, "corner_hole_shell");
  body->shells.push_back(shell);
  Face* face = model.make_face(surf, Orientation::Forward, "corner_hole_face");
  shell->faces.push_back(face);

  const Point3d o0{0, 0, 0}, o1{1, 0, 0}, o2{1, 1, 0}, o3{0, 1, 0};
  Vertex* corner = model.make_vertex(model.make_point(o0), tol, "corner");
  Vertex* ov1 = model.make_vertex(model.make_point(o1), tol, "ov1");
  Vertex* ov2 = model.make_vertex(model.make_point(o2), tol, "ov2");
  Vertex* ov3 = model.make_vertex(model.make_point(o3), tol, "ov3");
  Edge* oe[4] = {
      model.make_edge(model.make_line(o0, o1), corner, ov1, 0, 1, tol, "oe0"),
      model.make_edge(model.make_line(o1, o2), ov1, ov2, 0, 1, tol, "oe1"),
      model.make_edge(model.make_line(o2, o3), ov2, ov3, 0, 1, tol, "oe2"),
      model.make_edge(model.make_line(o3, o0), ov3, corner, 0, 1, tol, "oe3"),
  };
  Loop* outer = model.make_loop(face, LoopType::Outer, "outer");
  std::array<CoEdge*, 4> oces{};
  for (int i = 0; i < 4; ++i)
  {
    oces[static_cast<std::size_t>(i)] =
        model.make_coedge(oe[i], Orientation::Forward);
  }
  Model::link_loop(outer, oces);

  // Inner CW: (0,0) → (0.5,0) → (0,0.5); shares corner Vertex* with outer.
  const Point3d i1{0.5, 0, 0}, i2{0, 0.5, 0};
  Vertex* iv1 = model.make_vertex(model.make_point(i1), tol, "iv1");
  Vertex* iv2 = model.make_vertex(model.make_point(i2), tol, "iv2");
  Edge* ie[3] = {
      model.make_edge(model.make_line(o0, i1), corner, iv1, 0, 0.5, tol, "ie0"),
      model.make_edge(model.make_line(i1, i2), iv1, iv2, 0,
                     std::sqrt(0.5), tol, "ie1"),
      model.make_edge(model.make_line(i2, o0), iv2, corner, 0, 0.5, tol, "ie2"),
  };
  Loop* inner = model.make_loop(face, LoopType::Inner, "inner");
  std::array<CoEdge*, 3> ices{};
  for (int i = 0; i < 3; ++i)
  {
    ices[static_cast<std::size_t>(i)] =
        model.make_coedge(ie[i], Orientation::Forward);
  }
  Model::link_loop(inner, ices);

  return body;
}

TEST(TessellateInner, CornerTouchingHoleNotCovered)
{
  Model model;
  Body* body = make_corner_touching_hole_sheet(model);
  ASSERT_NE(body, nullptr);

  const TriangleMesh mesh = tessellate_body(*body);
  ASSERT_FALSE(mesh.indices.empty());

  const Point3d inside_hole{0.15, 0.15, 0};
  const Point3d outside{0.8, 0.8, 0};
  bool hole_covered = false;
  bool outside_covered = false;
  for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
  {
    const Point3d& a = mesh.vertices[mesh.indices[i]].position;
    const Point3d& b = mesh.vertices[mesh.indices[i + 1]].position;
    const Point3d& c = mesh.vertices[mesh.indices[i + 2]].position;
    if (point_in_triangle(inside_hole, a, b, c)) hole_covered = true;
    if (point_in_triangle(outside, a, b, c)) outside_covered = true;
  }

  EXPECT_FALSE(hole_covered)
      << "corner-touching hole interior should not lie in any triangle";
  EXPECT_TRUE(outside_covered) << "solid region should remain covered";
}

}  // namespace
}  // namespace brep
