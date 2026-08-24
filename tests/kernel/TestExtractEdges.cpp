#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(ExtractEdges, SphereHidesSeamByDefault)
{
  Model model;
  Body* body = MakeSphere(
      model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.5, .Name = "s"});
  ASSERT_NE(body, nullptr);

  const EdgeMesh hidden = extract_edges(*body);
  EXPECT_TRUE(hidden.Positions.empty());

  EdgeExtractionOptions opts;
  opts.include_seam_edges = true;
  const EdgeMesh shown = extract_edges(*body, opts);
  ASSERT_EQ(shown.Positions.size(), 2u);  // one segment = two endpoints
  const Point3d south{0, -1.5, 0};
  const Point3d north{0, 1.5, 0};
  const bool order_a =
      (shown.Positions[0] - south).norm() < 1e-9 &&
      (shown.Positions[1] - north).norm() < 1e-9;
  const bool order_b =
      (shown.Positions[0] - north).norm() < 1e-9 &&
      (shown.Positions[1] - south).norm() < 1e-9;
  EXPECT_TRUE(order_a || order_b);
}

TEST(ExtractEdges, BoxEdgesUnchanged)
{
  Model model;
  Body* body =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "b"});
  ASSERT_NE(body, nullptr);

  const EdgeMesh edges = extract_edges(*body);
  EXPECT_EQ(edges.Positions.size() / 2, 12u);
}

}  // namespace
}  // namespace brep
