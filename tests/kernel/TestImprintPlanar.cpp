#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/ImprintEngine.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/bool/Pipeline.h"
#include "brep/bool/TopologyCopy.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace brep
{
namespace
{

[[nodiscard]] std::size_t CountUniqueEdges(const Body& body)
{
  std::unordered_set<const Edge*> edges;
  for (const Shell* shell : body.Shells)
  {
    if (!shell)
    {
      continue;
    }
    for (const Face* face : shell->Faces)
    {
      if (!face)
      {
        continue;
      }
      for (const Loop* loop : face->Loops)
      {
        if (!loop || !loop->First)
        {
          continue;
        }
        CoEdge* coedge = loop->First;
        std::size_t guard = 0;
        do
        {
          if (++guard > 1024U)
          {
            break;
          }
          if (coedge->Edge)
          {
            edges.insert(coedge->Edge);
          }
          coedge = coedge->Next;
        } while (coedge && coedge != loop->First);
      }
    }
  }
  return edges.size();
}

TEST(ImprintPlanar, SplitEdgeAtDoublesEdgeCountOnBoxEdge)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
  ASSERT_NE(box, nullptr);
  ASSERT_NE(box->OuterShell(), nullptr);
  ASSERT_FALSE(box->OuterShell()->Faces.empty());

  Edge* targetEdge = box->OuterShell()->Faces.front()->OuterLoop()->First->Edge;
  ASSERT_NE(targetEdge, nullptr);
  ASSERT_NE(targetEdge->Curve, nullptr);
  ASSERT_EQ(targetEdge->Curve->Kind(), CurveKind::Line);

  const double midParam = 0.5 * (targetEdge->T0 + targetEdge->T1);
  const boolean::SplitEdgeResult split =
      boolean::SplitEdgeAt(model, *targetEdge, midParam, 1e-9);
  ASSERT_TRUE(split.Ok);
  ASSERT_NE(split.SplitVertex, nullptr);
  ASSERT_NE(split.SecondHalf, nullptr);
  EXPECT_EQ(CountUniqueEdges(*box), 13u);
}

TEST(ImprintPlanar, ManualChordBetweenCornersOnZMinFace)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
  ASSERT_NE(box, nullptr);
  Face* face = nullptr;
  for (Face* candidate : box->OuterShell()->Faces)
  {
    if (!candidate || !candidate->Surface ||
        candidate->Surface->Kind() != SurfaceKind::Plane)
    {
      continue;
    }
    const auto& plane = static_cast<const PlaneSurface&>(*candidate->Surface);
    if (std::abs(plane.Origin().z() - 0.0) < 1e-6 &&
        std::abs(plane.Normal(0, 0).z() + 1.0) < 1e-6)
    {
      face = candidate;
      break;
    }
  }
  ASSERT_NE(face, nullptr);

  const std::size_t before = CountUniqueEdges(*box);
  EXPECT_TRUE(boolean::ImprintSegmentOnPlanarFace(model, *box->OuterShell(), *face,
                                                  Point3d{0, 0.5, 0},
                                                  Point3d{1, 0.5, 0}, 1e-9));
  EXPECT_GT(CountUniqueEdges(*box), before);
}

TEST(ImprintPlanar, ManualChordOnBoxFaceIncreasesEdges)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});
  ASSERT_NE(box, nullptr);
  Face* face = nullptr;
  for (Face* candidate : box->OuterShell()->Faces)
  {
    if (!candidate || !candidate->Surface ||
        candidate->Surface->Kind() != SurfaceKind::Plane)
    {
      continue;
    }
    const auto& plane = static_cast<const PlaneSurface&>(*candidate->Surface);
    if (std::abs(plane.Origin().x() - 1.0) < 1e-6 &&
        std::abs(plane.Normal(0, 0).x() - 1.0) < 1e-6)
    {
      face = candidate;
      break;
    }
  }
  ASSERT_NE(face, nullptr);

  const std::size_t before = CountUniqueEdges(*box);
  EXPECT_TRUE(boolean::ImprintSegmentOnPlanarFace(model, *box->OuterShell(), *face,
                                                  Point3d{1, 0, 0.5},
                                                  Point3d{1, 1, 0.5}, 1e-9));
  EXPECT_GT(CountUniqueEdges(*box), before);
}

TEST(ImprintPlanar, TwoBoxesIncreaseEdgeCountAfterTopologicalImprint)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* boxB = MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  ASSERT_NE(boxA, nullptr);
  ASSERT_NE(boxB, nullptr);

  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *boxA, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingB = boolean::CopyBodySubgraph(ctxB, *boxB, "_wkB");
  ASSERT_NE(workingA, nullptr);
  ASSERT_NE(workingB, nullptr);

  const std::size_t edgesBefore =
      CountUniqueEdges(*workingA) + CountUniqueEdges(*workingB);

  boolean::PipelineState state;
  state.BodyA = boxA;
  state.BodyB = boxB;
  state.WorkingBodyA = workingA;
  state.WorkingBodyB = workingB;
  state.TargetModel = &model;
  state.IntersectionGraph = boolean::BuildIntersectionGraph(*workingA, *workingB);
  ASSERT_FALSE(state.IntersectionGraph.Segments.empty());

  const boolean::ImprintResult imprint = boolean::RunTopologicalImprint(state);
  ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;
  EXPECT_FALSE(state.IntersectionGraph.Fragments.empty());

  const std::size_t edgesAfter =
      CountUniqueEdges(*workingA) + CountUniqueEdges(*workingB);
  EXPECT_GT(edgesAfter, edgesBefore);
}

TEST(ImprintMixed, OctantSphereImprintsBoxPlaneFaces)
{
    Model model;
    Body* box =
        MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
    Body* sphere = MakeSphere(
        model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
    ASSERT_NE(box, nullptr);
    ASSERT_NE(sphere, nullptr);

    boolean::TopologyCopyContext boxCopy{model};
    Body* workingBox = boolean::CopyBodySubgraph(boxCopy, *box, "_wk");
    boolean::TopologyCopyContext sphereCopy{model};
    Body* workingSphere =
        boolean::CopyBodySubgraph(sphereCopy, *sphere, "_wk");
    ASSERT_NE(workingBox, nullptr);
    ASSERT_NE(workingSphere, nullptr);

    const std::size_t boxEdgesBefore = CountUniqueEdges(*workingBox);
    const std::size_t sphereEdgesBefore = CountUniqueEdges(*workingSphere);

    boolean::PipelineState state;
    state.BodyA = box;
    state.BodyB = sphere;
    state.WorkingBodyA = workingBox;
    state.WorkingBodyB = workingSphere;
    state.TargetModel = &model;
    state.IntersectionGraph =
        boolean::BuildIntersectionGraph(*workingBox, *workingSphere);
    ASSERT_FALSE(state.IntersectionGraph.Segments.empty());

    const boolean::ImprintResult imprint = boolean::RunImprint(state);
    ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;
    EXPECT_GT(CountUniqueEdges(*workingBox), boxEdgesBefore);
    EXPECT_GT(workingBox->OuterShell()->Faces.size(), 6U);
    EXPECT_GT(CountUniqueEdges(*workingSphere), sphereEdgesBefore);
    EXPECT_EQ(workingSphere->OuterShell()->Faces.size(), 2U);
    int sphereArcUses = 0;
    for (const Face* face : workingSphere->OuterShell()->Faces)
    {
        for (const Loop* loop : face->Loops)
        {
            loop->ForEachCoedge([&](const CoEdge& coedge)
            {
                if (coedge.Edge != nullptr &&
                    coedge.Edge->Name.find("_sphere_imprint_edge") !=
                        std::string::npos)
                {
                    ++sphereArcUses;
                    EXPECT_NE(coedge.Pcurve, nullptr);
                }
            });
        }
    }
    EXPECT_EQ(sphereArcUses, 6);
    EXPECT_TRUE(ValidateBody(*workingBox).Ok());
    EXPECT_TRUE(ValidateBody(*workingSphere).Ok());
}

TEST(ImprintSphere, ClosedCircleAwayFromSeamSplitsFace)
{
    Model model;
    Body* sphere = MakeSphere(
        model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
    ASSERT_NE(sphere, nullptr);
    Shell* shell = sphere->OuterShell();
    ASSERT_NE(shell, nullptr);
    ASSERT_EQ(shell->Faces.size(), 1U);
    Face* face = shell->Faces.front();
    ASSERT_NE(face, nullptr);

    const Point3d circleCenter{0, 0, 0.5};
    const double circleRadius = std::sqrt(0.75);
    ASSERT_TRUE(boolean::ImprintClosedCircleOnSphereFace(
        model, *shell, *face, circleCenter, Vector3d{0, 0, 1},
        circleRadius, 1e-7));

    ASSERT_EQ(shell->Faces.size(), 2U);
    EXPECT_EQ(face->Loops.size(), 2U);
    EXPECT_EQ(shell->Faces.back()->Loops.size(), 1U);
    EXPECT_EQ(CountUniqueEdges(*sphere), 3U);
    Loop* capLoop = shell->Faces.back()->OuterLoop();
    ASSERT_NE(capLoop, nullptr);
    capLoop->ForEachCoedge([&](const CoEdge& coedge)
    {
        ASSERT_NE(coedge.Edge, nullptr);
        ASSERT_NE(coedge.Edge->Curve, nullptr);
        EXPECT_EQ(coedge.Edge->Radial.size(), 2U);
        const Point3d midpoint = coedge.Edge->Curve->Eval(
            0.5 * (coedge.Edge->T0 + coedge.Edge->T1));
        EXPECT_NEAR((midpoint - Point3d{}).norm(), 1.0, 1e-7);
        EXPECT_NEAR(midpoint.z(), 0.5, 1e-7);
    });
    EXPECT_TRUE(ValidateBody(*sphere).Ok());
}

TEST(ImprintSphereSphere, ClosedCircleSplitsBothFaces)
{
    Model model;
    Body* sphereA = MakeSphere(
        model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "A"});
    Body* sphereB = MakeSphere(
        model, SphereSpec{.Center = {0, 0, 1}, .Radius = 1.0, .Name = "B"});
    ASSERT_NE(sphereA, nullptr);
    ASSERT_NE(sphereB, nullptr);

    boolean::TopologyCopyContext copyA{model};
    Body* workingA = boolean::CopyBodySubgraph(copyA, *sphereA, "_wk");
    boolean::TopologyCopyContext copyB{model};
    Body* workingB = boolean::CopyBodySubgraph(copyB, *sphereB, "_wk");
    ASSERT_NE(workingA, nullptr);
    ASSERT_NE(workingB, nullptr);

    boolean::PipelineState state;
    state.BodyA = sphereA;
    state.BodyB = sphereB;
    state.WorkingBodyA = workingA;
    state.WorkingBodyB = workingB;
    state.TargetModel = &model;
    state.IntersectionGraph =
        boolean::BuildIntersectionGraph(*workingA, *workingB);
    ASSERT_EQ(state.IntersectionGraph.Circles.size(), 1U);
    EXPECT_TRUE(state.IntersectionGraph.Circles.front().Closed);

    const boolean::ImprintResult imprint = boolean::RunImprint(state);
    ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;
    EXPECT_EQ(workingA->OuterShell()->Faces.size(), 2U);
    EXPECT_EQ(workingB->OuterShell()->Faces.size(), 2U);
    EXPECT_EQ(CountUniqueEdges(*workingA), 3U);
    EXPECT_EQ(CountUniqueEdges(*workingB), 3U);
    EXPECT_TRUE(ValidateBody(*workingA).Ok());
    EXPECT_TRUE(ValidateBody(*workingB).Ok());
}

TEST(ImprintPlanar, SlabUnionGraphVerticalSegmentOnZmax)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* boxB = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *boxA, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingB = boolean::CopyBodySubgraph(ctxB, *boxB, "_wkB");

  const boolean::IntersectionGraph graph =
      boolean::BuildIntersectionGraph(*workingA, *workingB);
  const double eps = 1e-7;
  bool found = false;
  for (const boolean::IntersectionSegment& segment : graph.Segments)
  {
    if (segment.FaceA == nullptr ||
        segment.FaceA->Name.find("zmax") == std::string::npos)
    {
      continue;
    }
    const double dx = std::abs(segment.Start.x() - segment.End.x());
    const double dy = std::abs(segment.Start.y() - segment.End.y());
    if (dx > eps || dy <= eps)
    {
      continue;
    }
    const double segX = 0.5 * (segment.Start.x() + segment.End.x());
    if (std::abs(segX - 1.0) > eps)
    {
      continue;
    }
    found = true;
    SCOPED_TRACE("start=" + std::to_string(segment.Start.x()) + "," +
                 std::to_string(segment.Start.y()) + "," +
                 std::to_string(segment.Start.z()) + " end=" +
                 std::to_string(segment.End.x()) + "," +
                 std::to_string(segment.End.y()) + "," +
                 std::to_string(segment.End.z()));
    const std::size_t before = workingA->Shells[0]->Faces.size();
    Shell* shellA = nullptr;
    for (Shell* shell : workingA->Shells)
    {
      if (shell && std::find(shell->Faces.begin(), shell->Faces.end(), segment.FaceA) !=
                      shell->Faces.end())
      {
        shellA = shell;
        break;
      }
    }
    ASSERT_NE(shellA, nullptr);
    Face* zmaxByName = nullptr;
    for (Face* face : workingA->Shells[0]->Faces)
    {
      if (face && face->Name.find("zmax") != std::string::npos)
      {
        zmaxByName = face;
        break;
      }
    }
    ASSERT_NE(zmaxByName, nullptr);
    EXPECT_EQ(segment.FaceA, zmaxByName);
    const bool ok = boolean::ImprintSegmentOnPlanarFace(
        model, *shellA, *segment.FaceA, segment.Start, segment.End, eps);
    EXPECT_TRUE(ok);
    EXPECT_GT(workingA->Shells[0]->Faces.size(), before);
    break;
  }
  EXPECT_TRUE(found) << "no vertical y-aligned segment on A zmax in graph";
}

TEST(ImprintPlanar, SlabUnionVerticalChordSplitsZmax)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *boxA, "_wkA");
  ASSERT_NE(workingA, nullptr);

  Face* zmax = nullptr;
  for (Face* face : workingA->Shells[0]->Faces)
  {
    if (face && face->Name.find("zmax") != std::string::npos)
    {
      zmax = face;
      break;
    }
  }
  ASSERT_NE(zmax, nullptr);

  const std::size_t facesBefore = workingA->Shells[0]->Faces.size();
  const bool ok = boolean::ImprintSegmentOnPlanarFace(
      model, *workingA->Shells[0], *zmax, Point3d{1, 1, 1}, Point3d{1, 0, 1}, 1e-7);
  EXPECT_TRUE(ok);
  EXPECT_GT(workingA->Shells[0]->Faces.size(), facesBefore);

  boolean::TopologyCopyContext ctxA2{model};
  Body* workingA2 = boolean::CopyBodySubgraph(ctxA2, *boxA, "_wkA2");
  Face* zmax2 = nullptr;
  for (Face* face : workingA2->Shells[0]->Faces)
  {
    if (face && face->Name.find("zmax") != std::string::npos)
    {
      zmax2 = face;
      break;
    }
  }
  ASSERT_NE(zmax2, nullptr);
  const bool okFuzzy = boolean::ImprintSegmentOnPlanarFace(
      model, *workingA2->Shells[0], *zmax2, Point3d{1, 1, 1}, Point3d{1, -1e-7, 1}, 1e-7);
  EXPECT_TRUE(okFuzzy);
  EXPECT_GT(workingA2->Shells[0]->Faces.size(), 6U);
}

TEST(ImprintPlanar, SlabUnionTopologicalImprintSplitsFaces)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* boxB = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *boxA, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingB = boolean::CopyBodySubgraph(ctxB, *boxB, "_wkB");
  ASSERT_NE(workingA, nullptr);
  ASSERT_NE(workingB, nullptr);

  boolean::PipelineState state;
  state.BodyA = boxA;
  state.BodyB = boxB;
  state.WorkingBodyA = workingA;
  state.WorkingBodyB = workingB;
  state.TargetModel = &model;
  state.IntersectionGraph = boolean::BuildIntersectionGraph(*workingA, *workingB);
  ASSERT_FALSE(state.IntersectionGraph.Segments.empty());
  const boolean::ImprintResult imprint = boolean::RunTopologicalImprint(state);
  ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;
  EXPECT_GT(workingA->Shells[0]->Faces.size(), 6U);
  EXPECT_GT(workingB->Shells[0]->Faces.size(), 6U);
}

TEST(ImprintPlanar, ZminVerticalChordOnFreshBodyA)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *boxA, "_wkA");
  Face* zmin = nullptr;
  for (Face* face : workingA->Shells[0]->Faces)
  {
    if (face && face->Name.find("zmin") != std::string::npos)
    {
      zmin = face;
      break;
    }
  }
  ASSERT_NE(zmin, nullptr);
  const bool ok = boolean::ImprintSegmentOnPlanarFace(
      model, *workingA->Shells[0], *zmin, Point3d{1, 0, 0}, Point3d{1, 1, 0}, 1e-7);
  EXPECT_TRUE(ok);
  const ValidationReport report = ValidateBody(*workingA);
  for (const ValidationIssue& issue : report.Issues)
  {
    ADD_FAILURE() << issue.Where << ": " << issue.Message;
  }
  EXPECT_TRUE(report.Ok());
}

TEST(ImprintPlanar, ZminVerticalChordOnFreshBodyB)
{
  Model model;
  Body* boxB = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  boolean::TopologyCopyContext ctxB{model};
  Body* workingB = boolean::CopyBodySubgraph(ctxB, *boxB, "_wkB");
  Face* zmin = nullptr;
  for (Face* face : workingB->Shells[0]->Faces)
  {
    if (face && face->Name.find("zmin") != std::string::npos)
    {
      zmin = face;
      break;
    }
  }
  ASSERT_NE(zmin, nullptr);
  const bool ok = boolean::ImprintSegmentOnPlanarFace(
      model, *workingB->Shells[0], *zmin, Point3d{2, 0, 0}, Point3d{2, 1, 0}, 1e-7);
  EXPECT_TRUE(ok);
  const ValidationReport report = ValidateBody(*workingB);
  for (const ValidationIssue& issue : report.Issues)
  {
    ADD_FAILURE() << issue.Where << ": " << issue.Message;
  }
  EXPECT_TRUE(report.Ok());
}

}  // namespace
}  // namespace brep
