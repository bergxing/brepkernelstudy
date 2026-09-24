#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/bool/BooleanBuilder.h"
#include "brep/bool/FaceSelector.h"
#include "brep/bool/SolidClassifier.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <unordered_set>

namespace brep
{
namespace
{

[[nodiscard]] double AabbVolume(const Point3d& mn, const Point3d& mx)
{
  return (mx.x() - mn.x()) * (mx.y() - mn.y()) * (mx.z() - mn.z());
}

[[nodiscard]] std::pair<Point3d, Point3d> BodyAabb(const Body& body)
{
  Point3d mn{1e300, 1e300, 1e300};
  Point3d mx{-1e300, -1e300, -1e300};
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
        if (!loop)
        {
          continue;
        }
        loop->ForEachCoedge([&](const CoEdge& ce)
        {
          if (Vertex* v = ce.From())
          {
            const Point3d& p = v->Position();
            mn = Point3d{std::min(mn.x(), p.x()), std::min(mn.y(), p.y()),
                         std::min(mn.z(), p.z())};
            mx = Point3d{std::max(mx.x(), p.x()), std::max(mx.y(), p.y()),
                         std::max(mx.z(), p.z())};
          }
        });
      }
    }
  }
  return {mn, mx};
}

TEST(BoxBoolean, WorkingBodyCloneValidates)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  ASSERT_NE(a, nullptr);
  boolean::TopologyCopyContext ctx{model};
  Body* working = boolean::CopyBodySubgraph(ctx, *a, "_wk");
  ASSERT_NE(working, nullptr);
  EXPECT_TRUE(ValidateBody(*working).Ok());
}

TEST(BoxBoolean, CopyAllFacesViaBuildValidates)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  ASSERT_NE(a, nullptr);
  boolean::TopologyCopyContext wkCtx{model};
  Body* working = boolean::CopyBodySubgraph(wkCtx, *a, "_wk");
  ASSERT_NE(working, nullptr);

  boolean::FaceSelection sel;
  for (Face* face : working->Shells[0]->Faces)
  {
    sel.FromA.push_back(face);
  }

  const boolean::BooleanBuildResult built =
      boolean::BuildBooleanBody(model, boolean::BooleanOp::Union, sel, "copy_all");
  ASSERT_NE(built.OutputBody, nullptr) << built.Diagnostics;
  EXPECT_TRUE(ValidateBody(*built.OutputBody).Ok());
}

TEST(BoxBoolean, WorkingBodyAfterImprintValidates)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *a, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingB = boolean::CopyBodySubgraph(ctxB, *b, "_wkB");
  ASSERT_NE(workingA, nullptr);
  ASSERT_NE(workingB, nullptr);

  boolean::PipelineState state;
  state.BodyA = a;
  state.BodyB = b;
  state.WorkingBodyA = workingA;
  state.WorkingBodyB = workingB;
  state.TargetModel = &model;
  state.IntersectionGraph =
      boolean::BuildIntersectionGraph(*workingA, *workingB);
  const boolean::ImprintResult imprint = boolean::RunTopologicalImprint(state);
  ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;
  const ValidationReport reportA = ValidateBody(*workingA);
  for (const ValidationIssue& issue : reportA.Issues)
  {
    ADD_FAILURE() << "A: " << issue.Where << ": " << issue.Message;
  }
  const ValidationReport reportB = ValidateBody(*workingB);
  for (const ValidationIssue& issue : reportB.Issues)
  {
    ADD_FAILURE() << "B: " << issue.Where << ": " << issue.Message;
  }
  EXPECT_TRUE(reportA.Ok());
  EXPECT_TRUE(reportB.Ok());
}

[[nodiscard]] Point3d FaceSamplePoint(const Face& face)
{
  const Loop* outer = face.OuterLoop();
  if (outer == nullptr || outer->First == nullptr)
  {
    return face.Surface != nullptr ? face.Surface->Eval(0.25, 0.25) : Point3d{};
  }
  Vector3d sum;
  std::size_t count = 0;
  outer->ForEachCoedge([&](const CoEdge& coedge)
  {
    if (coedge.From() != nullptr)
    {
      const Point3d position = coedge.From()->Position();
      sum += Vector3d{position.x(), position.y(), position.z()};
      ++count;
    }
  });
  if (count == 0U)
  {
    return face.Surface->Eval(0.25, 0.25);
  }
  const Vector3d average = sum / static_cast<double>(count);
  return Point3d{average.x(), average.y(), average.z()};
}

TEST(BoxBoolean, BothOutImprintPartnersPairedOnCopy)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *a, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingB = boolean::CopyBodySubgraph(ctxB, *b, "_wkB");
  ASSERT_NE(workingA, nullptr);
  ASSERT_NE(workingB, nullptr);

  boolean::PipelineState state;
  state.BodyA = a;
  state.BodyB = b;
  state.WorkingBodyA = workingA;
  state.WorkingBodyB = workingB;
  state.TargetModel = &model;
  state.IntersectionGraph =
      boolean::BuildIntersectionGraph(*workingA, *workingB);
  ASSERT_TRUE(boolean::RunTopologicalImprint(state).Ok);

  const double eps = 1e-7;
  std::vector<boolean::FaceClassification> aVsB;
  std::vector<boolean::FaceClassification> bVsA;
  for (Face* face : workingA->Shells[0]->Faces)
  {
    const boolean::FaceRegion region = boolean::SolidClassToFaceRegion(
        boolean::ClassifyPointInBody(*workingB, FaceSamplePoint(*face), eps));
    aVsB.push_back({face, region});
  }
  for (Face* face : workingB->Shells[0]->Faces)
  {
    const boolean::FaceRegion region = boolean::SolidClassToFaceRegion(
        boolean::ClassifyPointInBody(*workingA, FaceSamplePoint(*face), eps));
    bVsA.push_back({face, region});
  }

  const boolean::FaceSelection selection =
      boolean::SelectCsgFaces(boolean::BooleanOp::Union, aVsB, bVsA);
  std::unordered_set<const Face*> selectedFaces;
  for (Face* face : selection.FromA)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }
  for (Face* face : selection.FromB)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }

  boolean::TopologyCopyContext copyCtx{model};
  Body* body = model.MakeBody(BodyType::Solid, "pair_test");
  Shell* shell = model.MakeShell(true, "pair_test_shell");
  body->Shells.push_back(shell);
  for (Face* face : selection.FromA)
  {
    if (face != nullptr)
    {
      shell->Faces.push_back(boolean::CopyFaceSubgraph(copyCtx, *face, false,
                                                       &selectedFaces));
    }
  }
  for (Face* face : selection.FromB)
  {
    if (face != nullptr)
    {
      shell->Faces.push_back(boolean::CopyFaceSubgraph(copyCtx, *face, false,
                                                       &selectedFaces));
    }
  }
  boolean::PairAllCopiedCoedgePartners(copyCtx);

  const boolean::ImprintCopyPairingReport report =
      boolean::ReportImprintCopyPairing(selection, copyCtx);
  if (report.BothSelectedImprintCoedges == 0)
  {
    GTEST_SKIP() << "no both-selected imprint coedges with Out-only union selection";
  }
  EXPECT_EQ(report.MappedBothEnds, report.BothSelectedImprintCoedges);
  EXPECT_EQ(report.PairedCopiedCoedges, report.BothSelectedImprintCoedges);
  EXPECT_EQ(report.EdgeMismatch, 0);
}

TEST(BoxBoolean, PartnerUnselectedSeamExpansionCoversAll)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  boolean::TopologyCopyContext ctxA{model};
  Body* workingA = boolean::CopyBodySubgraph(ctxA, *a, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingB = boolean::CopyBodySubgraph(ctxB, *b, "_wkB");
  ASSERT_NE(workingA, nullptr);
  ASSERT_NE(workingB, nullptr);

  boolean::PipelineState state;
  state.BodyA = a;
  state.BodyB = b;
  state.WorkingBodyA = workingA;
  state.WorkingBodyB = workingB;
  state.TargetModel = &model;
  state.IntersectionGraph =
      boolean::BuildIntersectionGraph(*workingA, *workingB);
  ASSERT_TRUE(boolean::RunTopologicalImprint(state).Ok);

  const double eps = 1e-7;
  std::vector<boolean::FaceClassification> aVsB;
  std::vector<boolean::FaceClassification> bVsA;
  for (Face* face : workingA->Shells[0]->Faces)
  {
    aVsB.push_back({face, boolean::SolidClassToFaceRegion(
                                boolean::ClassifyPointInBody(*workingB,
                                                             FaceSamplePoint(*face),
                                                             eps))});
  }
  for (Face* face : workingB->Shells[0]->Faces)
  {
    bVsA.push_back({face, boolean::SolidClassToFaceRegion(
                                boolean::ClassifyPointInBody(*workingA,
                                                             FaceSamplePoint(*face),
                                                             eps))});
  }

  const boolean::FaceSelection selection =
      boolean::SelectCsgFaces(boolean::BooleanOp::Union, aVsB, bVsA);
  const boolean::SeamExpansionReport seamReport =
      boolean::ReportSeamExpansion(selection);
  EXPECT_GT(seamReport.PartnerUnselected, 0);
  EXPECT_EQ(seamReport.Expanded + seamReport.PreservedForSelectedBoundary,
            seamReport.PartnerUnselected);
  EXPECT_EQ(seamReport.CopiedImprintFallback, 0) << "fallbacks: "
                                                 << [&seamReport]()
  {
    std::string names;
    for (const std::string& name : seamReport.FallbackImprintNames)
    {
      names += name + " ";
    }
    return names;
  }();

  std::unordered_set<const Face*> selectedFaces;
  for (Face* face : selection.FromA)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }
  for (Face* face : selection.FromB)
  {
    if (face != nullptr)
    {
      selectedFaces.insert(face);
    }
  }
  boolean::TopologyCopyContext copyCtx{model};
  Body* body = model.MakeBody(BodyType::Solid, "seam_test");
  Shell* shell = model.MakeShell(true, "seam_test_shell");
  body->Shells.push_back(shell);
  for (Face* face : selection.FromA)
  {
    if (face != nullptr)
    {
      shell->Faces.push_back(
          boolean::CopyFaceSubgraph(copyCtx, *face, false, &selectedFaces));
    }
  }
  for (Face* face : selection.FromB)
  {
    if (face != nullptr)
    {
      shell->Faces.push_back(
          boolean::CopyFaceSubgraph(copyCtx, *face, false, &selectedFaces));
    }
  }
  boolean::PairAllCopiedCoedgePartners(copyCtx);

  int danglingImprints = 0;
  int loopGaps = 0;
  for (Face* face : shell->Faces)
  {
    if (face == nullptr)
    {
      continue;
    }
    for (Loop* loop : face->Loops)
    {
      if (loop == nullptr)
      {
        continue;
      }
      loop->ForEachCoedge([&](CoEdge& coedge)
      {
        if (coedge.Name.find("_imprint") != std::string::npos &&
            coedge.Partner == nullptr)
        {
          ++danglingImprints;
        }
        if (coedge.Next != nullptr && coedge.To() != coedge.Next->From())
        {
          ++loopGaps;
          ADD_FAILURE() << face->Name << " gap " << coedge.Name << " -> "
                        << coedge.Next->Name;
        }
      });
    }
  }
  EXPECT_EQ(loopGaps, 0);
  EXPECT_EQ(danglingImprints, seamReport.PreservedForSelectedBoundary);
}

TEST(BoxBoolean, UnionOverlappingValidationDump)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  (void)eval->Evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  const Body* built = nullptr;
  for (const auto& body : model.Bodies())
  {
    if (body && body->Name == "bool_A_B")
    {
      built = body.get();
      break;
    }
  }
  ASSERT_NE(built, nullptr) << "bool_A_B not found in model";
  const ValidationReport report = ValidateBody(*built);
  for (const ValidationIssue& issue : report.Issues)
  {
    ADD_FAILURE() << issue.Where << ": " << issue.Message;
  }
}

TEST(BoxBoolean, UnionOverlappingValidates)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Union, model, *a, *b, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  const auto report = ValidateBody(*result.OutputBody);
  EXPECT_TRUE(report.Ok());
  auto [mn, mx] = BodyAabb(*result.OutputBody);
  EXPECT_NEAR(mn.x(), 0.0, 1e-9);
  EXPECT_NEAR(mx.x(), 3.0, 1e-9);
  EXPECT_NEAR(AabbVolume(mn, mx), 3.0, 1e-9);
}

TEST(BoxBoolean, IntersectOverlappingIsBox)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *a, *b, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  auto [mn, mx] = BodyAabb(*result.OutputBody);
  EXPECT_NEAR(mn.x(), 1.0, 1e-9);
  EXPECT_NEAR(mn.y(), 1.0, 1e-9);
  EXPECT_NEAR(mn.z(), 1.0, 1e-9);
  EXPECT_NEAR(mx.x(), 2.0, 1e-9);
  EXPECT_NEAR(mx.y(), 2.0, 1e-9);
  EXPECT_NEAR(mx.z(), 2.0, 1e-9);
}

TEST(BoxBoolean, IntersectDisjointFailsWithDiagnostics)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *a, *b, {});
  EXPECT_FALSE(result.Ok());
  EXPECT_EQ(result.OutputBody, nullptr);
  EXPECT_FALSE(result.Diagnostics.empty());
}

TEST(BoxBoolean, SubtractPartialYieldsLShape)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {1, 1, 0}, .Max = {2, 2, 1}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *a, *b, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  auto [mn, mx] = BodyAabb(*result.OutputBody);
  EXPECT_NEAR(AabbVolume(mn, mx), 4.0, 1e-9);
  ASSERT_FALSE(result.OutputBody->Shells.empty());
  EXPECT_GT(result.OutputBody->Shells[0]->Faces.size(), 6u);
}

TEST(BoxBoolean, SubtractContainedFailsEmpty)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {2, 2, 2}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {3, 3, 3}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *a, *b, {});
  EXPECT_FALSE(result.Ok());
  EXPECT_FALSE(result.Diagnostics.empty());
}

TEST(BoxBoolean, SubtractNoOverlapKeepsTarget)
{
  Model model;
  Body* a = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* b = MakeBox(model, BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *a, *b, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  auto [mn, mx] = BodyAabb(*result.OutputBody);
  EXPECT_NEAR(AabbVolume(mn, mx), 1.0, 1e-9);
}

TEST(BoxBoolean, PartAddBooleanUsesDefaultEvaluator)
{
  auto doc = Document::Create("box_bool_part");
  Part& part = doc->AddPart("Main");
  Body* a = part.AddBox(BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});
  Body* b = part.AddBox(BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  const auto tid = part.Features().FindByBody(a->Guid)->Id();
  const auto tool = part.Features().FindByBody(b->Guid)->Id();
  Body* out = part.AddBoolean(boolean::BooleanOp::Union, tid, tool, "Fuse");
  ASSERT_NE(out, nullptr);
  EXPECT_TRUE(ValidateBody(*out).Ok());
  EXPECT_EQ(part.Model().Bodies().size(), 1u);
}

}  // namespace
}  // namespace brep
