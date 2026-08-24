#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/ops/Profile.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(ExtrudeHoles, RectWithHoleProducesInnerLoops)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "plate_hole";
  spec.distance = 1.0;
  spec.plane = Plane::xy();
  // Outer CCW in UV
  spec.profile.outer = {
      Point2d{0, 0},
      Point2d{4, 0},
      Point2d{4, 4},
      Point2d{0, 4},
  };
  // Hole (will be extruded as Inner on caps)
  spec.profile.holes.push_back({
      Point2d{1, 1},
      Point2d{3, 1},
      Point2d{3, 3},
      Point2d{1, 3},
  });

  Body* body = ops::extrude(model, spec);
  ASSERT_NE(body, nullptr);
  ASSERT_FALSE(body->shells.empty());
  Shell* shell = body->shells[0];
  ASSERT_NE(shell, nullptr);

  // Caps: bottom + top each Outer+Inner; sides: 4 outer + 4 hole = 8
  EXPECT_GE(shell->faces.size(), 10u);

  int faces_with_inner = 0;
  for (Face* face : shell->faces)
  {
    ASSERT_NE(face, nullptr);
    if (!face->inner_loops().empty())
    {
      ++faces_with_inner;
      EXPECT_EQ(face->inner_loops().size(), 1u);
      EXPECT_EQ(face->loops.size(), 2u);
    }
  }
  EXPECT_EQ(faces_with_inner, 2) << "top and bottom should each have one Inner";

  const auto report = ValidateBody(*body);
  EXPECT_TRUE(report.Ok());
  for (const auto& issue : report.Issues)
  {
    EXPECT_NE(issue.Severity, ValidationIssue::IssueSeverity::Error)
        << issue.Where << ": " << issue.Message;
  }
}

TEST(ExtrudeHoles, NoHolesStillWorks)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "prism";
  spec.distance = 2.0;
  spec.plane = Plane::xy();
  spec.profile.outer = {
      Point2d{0, 0},
      Point2d{1, 0},
      Point2d{1, 1},
      Point2d{0, 1},
  };
  Body* body = ops::extrude(model, spec);
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(ValidateBody(*body).Ok());
}

}  // namespace
}  // namespace brep
