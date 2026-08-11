#include "brep/mesh/loop_sample.hpp"

#include "brep/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <numbers>

namespace {

TEST(LoopSample, QuarterCircleHasInteriorSamples) {
  brep::Model model;
  auto* circle =
      model.make_circle({0, 0, 0}, {0, 0, 1}, 1.0, "unit_circle");
  auto* v0 = model.make_vertex(model.make_point(circle->eval(0.0)));
  auto* v1 =
      model.make_vertex(model.make_point(circle->eval(std::numbers::pi / 2.0)));
  auto* edge =
      model.make_edge(circle, v0, v1, 0.0, std::numbers::pi / 2.0);
  auto* coedge = model.make_coedge(edge, brep::Orientation::Forward);

  const auto points = brep::mesh::sample_edge_xyz(*coedge, {});

  ASSERT_GE(points.size(), 4u);
  EXPECT_NEAR(points.front().distance_to(circle->eval(0.0)), 0.0, 1e-12);
  EXPECT_NEAR(points.back().distance_to(circle->eval(std::numbers::pi / 2.0)),
              0.0, 1e-12);
}

TEST(LoopSample, ReverseLineFollowsCoedgeSense) {
  brep::Model model;
  auto* v0 = model.make_vertex(model.make_point({1, 2, 3}));
  auto* v1 = model.make_vertex(model.make_point({4, 2, 3}));
  auto* edge =
      model.make_edge(model.make_line(v0->position(), v1->position()), v0, v1,
                      0.0, 3.0);
  auto* coedge = model.make_coedge(edge, brep::Orientation::Reversed);

  const auto points = brep::mesh::sample_edge_xyz(*coedge, {});

  ASSERT_EQ(points.size(), 2u);
  EXPECT_NEAR(points.front().distance_to(v1->position()), 0.0, 1e-12);
  EXPECT_NEAR(points.back().distance_to(v0->position()), 0.0, 1e-12);
}

TEST(LoopSample, PlaneLoopProjectsCornersWithoutClosingDuplicate) {
  brep::Model model;
  auto* plane =
      model.make_plane(brep::Point3d{10, 20, 30}, brep::Vector3d{1, 0, 0},
                       brep::Vector3d{0, 1, 0});
  auto* face = model.make_face(plane);
  auto* loop = model.make_loop(face, brep::LoopType::Inner);

  const std::array<brep::Point3d, 4> corners = {
      brep::Point3d{10, 20, 30}, brep::Point3d{12, 20, 30},
      brep::Point3d{12, 23, 30}, brep::Point3d{10, 23, 30}};
  std::array<brep::Vertex*, 4> vertices{};
  std::array<brep::CoEdge*, 4> coedges{};
  for (std::size_t i = 0; i < corners.size(); ++i) {
    vertices[i] = model.make_vertex(model.make_point(corners[i]));
  }
  for (std::size_t i = 0; i < corners.size(); ++i) {
    const std::size_t next = (i + 1) % corners.size();
    auto* line = model.make_line(corners[i], corners[next]);
    auto* edge =
        model.make_edge(line, vertices[i], vertices[next], 0.0, line->length());
    coedges[i] = model.make_coedge(edge, brep::Orientation::Forward);
  }
  brep::Model::link_loop(loop, coedges);

  const auto ring = brep::mesh::sample_loop(*loop, *plane, {});

  ASSERT_EQ(ring.type, brep::LoopType::Inner);
  ASSERT_EQ(ring.points.size(), 4u);
  EXPECT_DOUBLE_EQ(ring.points[0].uv.u(), 0.0);
  EXPECT_DOUBLE_EQ(ring.points[0].uv.v(), 0.0);
  EXPECT_DOUBLE_EQ(ring.points[2].uv.u(), 2.0);
  EXPECT_DOUBLE_EQ(ring.points[2].uv.v(), 3.0);
  EXPECT_GT(ring.points.front().xyz.distance_to(ring.points.back().xyz), 1.0);
}

TEST(LoopSample, SphereRingAcrossSeamIsContiguous) {
  using brep::Point2d;
  using brep::mesh::SampledRing;
  using brep::mesh::unwrap_sphere_ring;

  SampledRing r;
  // Three samples crossing the periodic seam near u=0 / u=2π.
  r.points = {
      {{}, Point2d{6.0, 0.1}},
      {{}, Point2d{6.2, 0.0}},   // near 2π
      {{}, Point2d{0.1, -0.1}},  // just past the seam
  };

  const auto u = unwrap_sphere_ring(r);
  ASSERT_EQ(u.points.size(), 3u);
  EXPECT_LT(std::abs(u.points[1].uv.u() - u.points[0].uv.u()),
            std::numbers::pi);
  EXPECT_LT(std::abs(u.points[2].uv.u() - u.points[1].uv.u()),
            std::numbers::pi);
}

}  // namespace
