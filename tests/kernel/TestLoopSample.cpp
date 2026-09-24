#include "brep/mesh/LoopSample.h"

#include "brep/Model.h"

#include <gtest/gtest.h>

#include <array>
#include <numbers>

namespace
{

TEST(LoopSample, QuarterCircleHasInteriorSamples)
{
  brep::Model model;
  auto* circle =
      model.MakeCircle({0, 0, 0}, {0, 0, 1}, 1.0, "unit_circle");
  auto* v0 = model.MakeVertex(model.MakePoint(circle->Eval(0.0)));
  auto* v1 =
      model.MakeVertex(model.MakePoint(circle->Eval(std::numbers::pi / 2.0)));
  auto* edge =
      model.MakeEdge(circle, v0, v1, 0.0, std::numbers::pi / 2.0);
  auto* coedge = model.MakeCoedge(edge, brep::Orientation::Forward);

  const auto points = brep::mesh::SampleEdgeXyz(*coedge, {});

  ASSERT_GE(points.size(), 4u);
  EXPECT_NEAR(points.front().distance_to(circle->Eval(0.0)), 0.0, 1e-12);
  EXPECT_NEAR(points.back().distance_to(circle->Eval(std::numbers::pi / 2.0)),
              0.0, 1e-12);
}

TEST(LoopSample, StaleCircleParamsFollowWeldedVertices)
{
  brep::Model model;
  auto* circle =
      model.MakeCircle({0, 0, 0}, {0, 0, 1}, 1.0, "unit_circle");
  const double tFrom = 0.1;
  const double tTo = 0.4;
  auto* v0 = model.MakeVertex(model.MakePoint(circle->Eval(tFrom)));
  auto* v1 = model.MakeVertex(model.MakePoint(circle->Eval(tTo)));
  // T0/T1 still describe a different arc on the same circle (boolean weld).
  auto* edge = model.MakeEdge(circle, v0, v1, std::numbers::pi,
                              std::numbers::pi * 1.5);
  auto* coedge = model.MakeCoedge(edge, brep::Orientation::Forward);

  const auto points = brep::mesh::SampleEdgeXyz(*coedge, {});

  ASSERT_GE(points.size(), 3u);
  EXPECT_NEAR(points.front().distance_to(v0->Position()), 0.0, 1e-12);
  EXPECT_NEAR(points.back().distance_to(v1->Position()), 0.0, 1e-12);
  const brep::Point3d mid = points[points.size() / 2];
  EXPECT_NEAR(mid.distance_to(circle->Center()), 1.0, 1e-9);
  EXPECT_LT(mid.distance_to(circle->Eval(0.25)), 0.15);
  EXPECT_GT(mid.distance_to(circle->Eval(std::numbers::pi * 1.25)), 1.0);
}

TEST(LoopSample, ReverseLineFollowsCoedgeSense)
{
  brep::Model model;
  auto* v0 = model.MakeVertex(model.MakePoint({1, 2, 3}));
  auto* v1 = model.MakeVertex(model.MakePoint({4, 2, 3}));
  auto* edge =
      model.MakeEdge(model.MakeLine(v0->Position(), v1->Position()), v0, v1,
                     0.0, 3.0);
  auto* coedge = model.MakeCoedge(edge, brep::Orientation::Reversed);

  const auto points = brep::mesh::SampleEdgeXyz(*coedge, {});

  ASSERT_EQ(points.size(), 2u);
  EXPECT_NEAR(points.front().distance_to(v1->Position()), 0.0, 1e-12);
  EXPECT_NEAR(points.back().distance_to(v0->Position()), 0.0, 1e-12);
}

TEST(LoopSample, PlaneLoopProjectsCornersWithoutClosingDuplicate)
{
  brep::Model model;
  auto* plane =
      model.MakePlane(brep::Point3d{10, 20, 30}, brep::Vector3d{1, 0, 0},
                      brep::Vector3d{0, 1, 0});
  auto* face = model.MakeFace(plane);
  auto* loop = model.MakeLoop(face, brep::LoopType::Inner);

  const std::array<brep::Point3d, 4> corners = {
      brep::Point3d{10, 20, 30}, brep::Point3d{12, 20, 30},
      brep::Point3d{12, 23, 30}, brep::Point3d{10, 23, 30}};
  std::array<brep::Vertex*, 4> vertices{};
  std::array<brep::CoEdge*, 4> coedges{};
  for (std::size_t i = 0; i < corners.size(); ++i)
  {
    vertices[i] = model.MakeVertex(model.MakePoint(corners[i]));
  }
  for (std::size_t i = 0; i < corners.size(); ++i)
  {
    const std::size_t next = (i + 1) % corners.size();
    auto* line = model.MakeLine(corners[i], corners[next]);
    auto* edge =
        model.MakeEdge(line, vertices[i], vertices[next], 0.0, line->Length());
    coedges[i] = model.MakeCoedge(edge, brep::Orientation::Forward);
  }
  brep::Model::LinkLoop(loop, coedges);

  const auto ring = brep::mesh::SampleLoop(*loop, *plane, {});

  ASSERT_EQ(ring.Type, brep::LoopType::Inner);
  ASSERT_EQ(ring.Points.size(), 4u);
  EXPECT_DOUBLE_EQ(ring.Points[0].Uv.u(), 0.0);
  EXPECT_DOUBLE_EQ(ring.Points[0].Uv.v(), 0.0);
  EXPECT_DOUBLE_EQ(ring.Points[2].Uv.u(), 2.0);
  EXPECT_DOUBLE_EQ(ring.Points[2].Uv.v(), 3.0);
  EXPECT_GT(ring.Points.front().Xyz.distance_to(ring.Points.back().Xyz), 1.0);
}

TEST(LoopSample, SphereRingAcrossSeamIsContiguous)
{
  using brep::Point2d;
  using brep::Point3d;
  using brep::mesh::SampledRing;
  using brep::mesh::UnwrapSphereRing;

  SampledRing ring;
  // Three samples crossing the periodic seam near u=0 / u=2π.
  // Distinct xyz required: identical points encode an intentional seam cut.
  ring.Points = {
      {Point3d{0.99, 0.05, -0.1}, Point2d{6.0, 0.1}},
      {Point3d{0.99, 0.0, -0.05}, Point2d{6.2, 0.0}},
      {Point3d{0.99, -0.05, 0.05}, Point2d{0.1, -0.1}},
  };

  const auto unwrapped = UnwrapSphereRing(ring);
  ASSERT_EQ(unwrapped.Points.size(), 3u);
  EXPECT_LT(std::abs(unwrapped.Points[1].Uv.u() - unwrapped.Points[0].Uv.u()),
            std::numbers::pi);
  EXPECT_LT(std::abs(unwrapped.Points[2].Uv.u() - unwrapped.Points[1].Uv.u()),
            std::numbers::pi);
}

}  // namespace
