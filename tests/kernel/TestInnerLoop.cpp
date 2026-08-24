#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/Geometry.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace brep
{
namespace
{

/// Sheet: planar XY face with outer [0,2]x[0,2] and inner hole.
/// Outer CCW (+Z); Inner CW (hole boundary).
Body* make_planar_face_with_hole(Model& model)
{
  constexpr double tol = 1e-7;
  PlaneSurface* surf =
      model.make_plane(Point3d{0, 0, 0}, Vector3d{1, 0, 0}, Vector3d{0, 1, 0},
                       "plate_surf");
  Body* body = model.make_body(BodyType::Sheet, "plate");
  Shell* shell = model.make_shell(false, "plate_shell");
  body->shells.push_back(shell);
  Face* face = model.make_face(surf, Orientation::Forward, "plate_face");
  shell->faces.push_back(face);

  const Point3d o0{0, 0, 0}, o1{2, 0, 0}, o2{2, 2, 0}, o3{0, 2, 0};
  Vertex* ov[4] = {
      model.make_vertex(model.make_point(o0), tol, "ov0"),
      model.make_vertex(model.make_point(o1), tol, "ov1"),
      model.make_vertex(model.make_point(o2), tol, "ov2"),
      model.make_vertex(model.make_point(o3), tol, "ov3"),
  };
  Edge* oe[4] = {
      model.make_edge(model.make_line(o0, o1), ov[0], ov[1], 0, 2, tol, "oe0"),
      model.make_edge(model.make_line(o1, o2), ov[1], ov[2], 0, 2, tol, "oe1"),
      model.make_edge(model.make_line(o2, o3), ov[2], ov[3], 0, 2, tol, "oe2"),
      model.make_edge(model.make_line(o3, o0), ov[3], ov[0], 0, 2, tol, "oe3"),
  };
  Loop* outer = model.make_loop(face, LoopType::Outer, "outer");
  std::array<CoEdge*, 4> oces{};
  for (int i = 0; i < 4; ++i)
  {
    oces[static_cast<std::size_t>(i)] =
        model.make_coedge(oe[i], Orientation::Forward);
  }
  Model::link_loop(outer, oces);

  // Hole CW: (0.5,0.5)->(0.5,1.5)->(1.5,1.5)->(1.5,0.5)
  const Point3d i0{0.5, 0.5, 0}, i1{0.5, 1.5, 0}, i2{1.5, 1.5, 0},
      i3{1.5, 0.5, 0};
  Vertex* iv[4] = {
      model.make_vertex(model.make_point(i0), tol, "iv0"),
      model.make_vertex(model.make_point(i1), tol, "iv1"),
      model.make_vertex(model.make_point(i2), tol, "iv2"),
      model.make_vertex(model.make_point(i3), tol, "iv3"),
  };
  Edge* ie[4] = {
      model.make_edge(model.make_line(i0, i1), iv[0], iv[1], 0, 1, tol, "ie0"),
      model.make_edge(model.make_line(i1, i2), iv[1], iv[2], 0, 1, tol, "ie1"),
      model.make_edge(model.make_line(i2, i3), iv[2], iv[3], 0, 1, tol, "ie2"),
      model.make_edge(model.make_line(i3, i0), iv[3], iv[0], 0, 1, tol, "ie3"),
  };
  Loop* inner = model.make_loop(face, LoopType::Inner, "inner");
  std::array<CoEdge*, 4> ices{};
  for (int i = 0; i < 4; ++i)
  {
    ices[static_cast<std::size_t>(i)] =
        model.make_coedge(ie[i], Orientation::Forward);
  }
  Model::link_loop(inner, ices);

  return body;
}

TEST(InnerLoop, PlanarFaceWithHoleValidates)
{
  Model model;
  Body* body = make_planar_face_with_hole(model);
  ASSERT_NE(body, nullptr);
  Face* face = body->shells[0]->faces[0];
  ASSERT_EQ(face->loops.size(), 2u);
  EXPECT_EQ(face->outer_loop()->type, LoopType::Outer);
  ASSERT_EQ(face->inner_loops().size(), 1u);
  EXPECT_EQ(face->inner_loops()[0]->type, LoopType::Inner);

  const auto report = ValidateBody(*body);
  EXPECT_TRUE(report.Ok());
  for (const auto& issue : report.Issues)
  {
    EXPECT_NE(issue.Severity, ValidationIssue::IssueSeverity::Error)
        << issue.Where << ": " << issue.Message;
  }
}

TEST(InnerLoop, MultipleOutersAllowed)
{
  Model model;
  Body* body = make_planar_face_with_hole(model);
  Face* face = body->shells[0]->faces[0];
  face->loops[1]->type = LoopType::Outer;  // two Outers, zero Inner

  const auto report = ValidateBody(*body);
  EXPECT_TRUE(report.Ok()) << "multi-outer must validate";
  ASSERT_EQ(face->outer_loops().size(), 2u);
}

TEST(InnerLoop, MissingOuterStillErrors)
{
  Model model;
  Body* body = make_planar_face_with_hole(model);
  Face* face = body->shells[0]->faces[0];
  for (Loop* l : face->loops) l->type = LoopType::Inner;
  EXPECT_FALSE(ValidateBody(*body).Ok());
}

TEST(InnerLoop, BoxStillValidatesWithSingleOuter)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
  ASSERT_NE(box, nullptr);
  EXPECT_TRUE(ValidateBody(*box).Ok());
}

}  // namespace
}  // namespace brep
