#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"
#include "brep/bool/Boolean.h"
#include "brep/build/PrimitiveBuild.h"

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

  const EdgeMesh hidden = ExtractEdges(*body);
  EXPECT_TRUE(hidden.Positions.empty());

  EdgeExtractionOptions opts;
  opts.IncludeSeamEdges = true;
  const EdgeMesh shown = ExtractEdges(*body, opts);
  ASSERT_GT(shown.Positions.size(), 2u);
  const Point3d south{0, -1.5, 0};
  const Point3d north{0, 1.5, 0};
  bool touches_south = false;
  bool touches_north = false;
  for (const Point3d& p : shown.Positions)
  {
    if ((p - south).norm() < 1e-6) touches_south = true;
    if ((p - north).norm() < 1e-6) touches_north = true;
  }
  EXPECT_TRUE(touches_south);
  EXPECT_TRUE(touches_north);
}

TEST(ExtractEdges, SphereBoxUnionInnerArcIsSampled)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{
                                   .Min = {-2.944, 0, 1.68354},
                                   .Max = {-2.13126, 0.826297, 2.35774},
                               });
  Body* sphere = MakeSphere(model, SphereSpec{
                                       .Center = {-2.13126, 0.826297, 2.35774},
                                       .Radius = 0.337101,
                                   });
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *box, *sphere, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  Body* body = result.OutputBody;
  ASSERT_NE(body, nullptr);

  const EdgeMesh edges = ExtractEdges(*body);
  ASSERT_GT(edges.Positions.size() / 2, 12u)
      << "union wireframe should include sampled circle arcs, not only chords";

  std::size_t circle_segments = 0;
  for (Shell* shell : body->Shells)
  {
    for (Face* face : shell->Faces)
    {
      if (!face) continue;
      for (Loop* loop : face->InnerLoops())
      {
        loop->ForEachCoedge([&](const CoEdge& ce)
        {
          if (!ce.Edge || !ce.Edge->Curve ||
              ce.Edge->Curve->Kind() != CurveKind::Circle)
          {
            return;
          }
          ++circle_segments;
        });
      }
    }
  }
  ASSERT_GE(circle_segments, 3u);
  EXPECT_GE(edges.Positions.size() / 2, circle_segments * 8u);
}

TEST(ExtractEdges, BoxEdgesUnchanged)
{
  Model model;
  Body* body =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "b"});
  ASSERT_NE(body, nullptr);

  const EdgeMesh edges = ExtractEdges(*body);
  EXPECT_EQ(edges.Positions.size() / 2, 12u);
}

}  // namespace
}  // namespace brep
