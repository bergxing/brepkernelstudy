#include "api/core.hpp"
#include "api/mesh.hpp"
#include "api/modeling.hpp"

#include <gtest/gtest.h>

namespace brep {
namespace {

TEST(ExtractEdges, SphereHidesSeamByDefault) {
  Model model;
  Body* body = make_sphere(
      model, SphereSpec{.center = {0, 0, 0}, .radius = 1.5, .name = "s"});
  ASSERT_NE(body, nullptr);

  const EdgeMesh hidden = extract_edges(*body);
  EXPECT_TRUE(hidden.positions.empty());

  EdgeExtractionOptions opts;
  opts.include_seam_edges = true;
  const EdgeMesh shown = extract_edges(*body, opts);
  ASSERT_EQ(shown.positions.size(), 2u);  // one segment = two endpoints
  const Point3d south{0, -1.5, 0};
  const Point3d north{0, 1.5, 0};
  const bool order_a =
      (shown.positions[0] - south).norm() < 1e-9 &&
      (shown.positions[1] - north).norm() < 1e-9;
  const bool order_b =
      (shown.positions[0] - north).norm() < 1e-9 &&
      (shown.positions[1] - south).norm() < 1e-9;
  EXPECT_TRUE(order_a || order_b);
}

TEST(ExtractEdges, BoxEdgesUnchanged) {
  Model model;
  Body* body =
      make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {1, 1, 1}, .name = "b"});
  ASSERT_NE(body, nullptr);

  const EdgeMesh edges = extract_edges(*body);
  EXPECT_EQ(edges.positions.size() / 2, 12u);
}

}  // namespace
}  // namespace brep
