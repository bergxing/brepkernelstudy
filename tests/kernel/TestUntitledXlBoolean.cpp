#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/bool/Boolean.h"
#include "brep/bool/FaceSelector.h"
#include "brep/bool/ImprintEngine.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/bool/Pipeline.h"
#include "brep/bool/TopologyCopy.h"
#include "brep/feat/BooleanFeature.h"
#include "brep/io/XlDocument.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/ops/Profile.h"
#include "brep/Mesh.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <unordered_map>

namespace brep
{
namespace
{

/// Corner pad + sphere matching untitled.xl (Pad 0da825c1…, sphere 3011fb97…).
struct CornerPadSphereFixture
{
  std::shared_ptr<Document> Document;
  Part* Part{nullptr};
  Body* Pad{nullptr};
  Body* Sphere{nullptr};
  feat::FeatureId PadFeatureId;
  feat::FeatureId SphereFeatureId;
};

[[nodiscard]] CornerPadSphereFixture MakeCornerPadSphereFixture()
{
  CornerPadSphereFixture fixture;
  fixture.Document =
      Document::Create("corner_pad_sphere", boolean::MakeDefaultBooleanEvaluator());
  fixture.Part = &fixture.Document->AddPart("Main");
  Model& model = fixture.Part->Model();

  ops::ExtrudeSpec padSpec;
  padSpec.Name = "Pad";
  padSpec.Plane = Plane::XzYUp();
  padSpec.Distance = 0.9996867964130517;
  padSpec.Symmetric = false;
  padSpec.Profile.Outer = {
      Point2d{3.53808, 6.51887},
      Point2d{4.03792, 6.51887},
      Point2d{4.03792, 7.51855},
      Point2d{3.53808, 7.51855},
  };
  fixture.Pad = ops::Extrude(model, padSpec);
  fixture.Sphere = MakeSphere(
      model,
      SphereSpec{.Center = {4.03792, 0.999687, 7.01871},
                 .Radius = 0.499843,
                 .Name = "sphere"});

  if (fixture.Pad != nullptr)
  {
    if (const feat::IFeature* feature =
            fixture.Part->Features().FindByBody(fixture.Pad->Guid))
    {
      fixture.PadFeatureId = feature->Id();
    }
  }
  if (fixture.Sphere != nullptr)
  {
    if (const feat::IFeature* feature =
            fixture.Part->Features().FindByBody(fixture.Sphere->Guid))
    {
      fixture.SphereFeatureId = feature->Id();
    }
  }

  return fixture;
}

[[nodiscard]] bool FixtureReady(const CornerPadSphereFixture& fixture)
{
  return fixture.Part != nullptr && fixture.Pad != nullptr &&
         fixture.Sphere != nullptr;
}

[[nodiscard]] const char* OptionalUntitledXlPath()
{
  if (const char* envPath = std::getenv("BREP_UNTITLED_XL_PATH"))
  {
    if (envPath[0] != '\0')
    {
      return envPath;
    }
  }
  return nullptr;
}

[[nodiscard]] Body* FindBodyByGuid(Part& part, const char* guidText)
{
  const Guid guid = Guid::FromString(guidText);
  if (!guid.IsValid())
  {
    return nullptr;
  }
  return part.FindBody(guid);
}

[[nodiscard]] bool SpherePatchHasPoleCorner(const Body& body)
{
  for (Shell* shell : body.Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (face == nullptr || face->Surface == nullptr ||
          face->Surface->Kind() != SurfaceKind::Sphere)
      {
        continue;
      }
      const auto* sphere = static_cast<const SphereSurface*>(face->Surface);
      const Point3d south{sphere->Center().x(),
                          sphere->Center().y() - sphere->Radius(),
                          sphere->Center().z()};
      const Point3d north{sphere->Center().x(),
                          sphere->Center().y() + sphere->Radius(),
                          sphere->Center().z()};
      if (face->OuterLoop() == nullptr)
      {
        continue;
      }
      bool hasPole = false;
      face->OuterLoop()->ForEachCoedge([&](const CoEdge& coedge)
      {
        for (const Vertex* vertex : {coedge.From(), coedge.To()})
        {
          if (vertex == nullptr)
          {
            continue;
          }
          if (vertex->Position().distance_to(south) <= 1e-4 ||
              vertex->Position().distance_to(north) <= 1e-4)
          {
            hasPole = true;
          }
        }
      });
      if (hasPole)
      {
        return true;
      }
    }
  }
  return false;
}

void DumpFaceLoopCoedges(const Face& face)
{
  std::cout << "FACE " << face.Name << "\n";
  for (const Loop* loop : face.Loops)
  {
    if (loop == nullptr || loop->First == nullptr)
    {
      continue;
    }
    std::cout << "  LOOP " << loop->Name << "\n";
    CoEdge* coedge = loop->First;
    std::size_t guard = 0;
    CoEdge* prev = nullptr;
    do
    {
      if (++guard > 64U)
      {
        break;
      }
      const char* edgeName =
          coedge->Edge != nullptr ? coedge->Edge->Name.c_str() : "(null)";
      const char* partnerFace =
          coedge->Partner != nullptr && coedge->Partner->GetFace() != nullptr
              ? coedge->Partner->GetFace()->Name.c_str()
              : "(null)";
      const char* fromName =
          coedge->From() != nullptr ? coedge->From()->Name.c_str() : "(null)";
      const char* toName =
          coedge->To() != nullptr ? coedge->To()->Name.c_str() : "(null)";
      const bool meets =
          prev == nullptr ||
          (prev->To() != nullptr && coedge->From() != nullptr &&
           prev->To() == coedge->From());
      std::cout << "    " << coedge->Name << " edge=" << edgeName
                << " partnerFace=" << partnerFace << " from=" << fromName
                << " to=" << toName << " meetsPrev=" << meets << "\n";
      prev = coedge;
      coedge = coedge->Next;
    } while (coedge != nullptr && coedge != loop->First);
  }
}

void ReportBooleanBodyValidationFailures(const Part& part)
{
  for (const auto& body : part.Model().Bodies())
  {
    if (body == nullptr || body->Name.rfind("bool_", 0) != 0)
    {
      continue;
    }
    const ValidationReport report = ValidateBody(*body);
    for (const ValidationIssue& issue : report.Issues)
    {
      if (issue.Severity == ValidationIssue::IssueSeverity::Error)
      {
        ADD_FAILURE() << issue.Where << ": " << issue.Message;
      }
    }
  }
}

void DumpEdgeRadial(Body& body)
{
  std::unordered_map<Edge*, std::size_t> radial;
  for (Shell* shell : body.Shells)
  {
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
        loop->ForEachCoedge([&](const CoEdge& coedge)
        {
          if (coedge.Edge != nullptr)
          {
            ++radial[coedge.Edge];
          }
        });
      }
    }
  }
  for (const auto& [edge, count] : radial)
  {
    if (count != 2U && edge != nullptr)
    {
      std::cout << "EDGE " << edge->Name << " radial=" << count << "\n";
    }
  }
}

}  // namespace

TEST(UntitledXlBoolean, ImprintCreatesSplitsOnCornerPad)
{
  const CornerPadSphereFixture fixture = MakeCornerPadSphereFixture();
  if (!FixtureReady(fixture))
  {
    GTEST_SKIP() << "failed to build corner pad/sphere fixture";
  }

  Model& model = fixture.Part->Model();
  boolean::TopologyCopyContext ctxA{model};
  Body* workingSphere = boolean::CopyBodySubgraph(ctxA, *fixture.Sphere, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingPad = boolean::CopyBodySubgraph(ctxB, *fixture.Pad, "_wkB");
  ASSERT_NE(workingSphere, nullptr);
  ASSERT_NE(workingPad, nullptr);

  boolean::PipelineState state;
  state.BodyA = fixture.Sphere;
  state.BodyB = fixture.Pad;
  state.WorkingBodyA = workingSphere;
  state.WorkingBodyB = workingPad;
  state.TargetModel = &model;
  state.Op = boolean::BooleanOp::Intersect;
  state.IntersectionGraph =
      boolean::BuildIntersectionGraph(*workingSphere, *workingPad);

  SCOPED_TRACE("segments=" + std::to_string(state.IntersectionGraph.Segments.size()) +
               " circles=" + std::to_string(state.IntersectionGraph.Circles.size()));

  const boolean::ImprintResult imprint = boolean::RunTopologicalImprint(state);
  ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;

  std::size_t padFaces = 0;
  std::size_t splitFaces = 0;
  for (Shell* shell : workingPad->Shells)
  {
    for (Face* face : shell->Faces)
    {
      ++padFaces;
      if (face != nullptr && face->Name.find("_split") != std::string::npos)
      {
        ++splitFaces;
      }
    }
  }
  EXPECT_GT(state.IntersectionGraph.Segments.size(), 0U);
  EXPECT_GT(splitFaces, 0U) << "padFaces=" << padFaces;
}

TEST(UntitledXlBoolean, PadIntersectSphereIfPresent)
{
  const CornerPadSphereFixture fixture = MakeCornerPadSphereFixture();
  if (!FixtureReady(fixture))
  {
    GTEST_SKIP() << "failed to build corner pad/sphere fixture";
  }

  SCOPED_TRACE("sphere center at pad corner (untitled.xl geometry)");

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result = eval->Evaluate(boolean::BooleanOp::Intersect,
                                     fixture.Part->Model(), *fixture.Sphere,
                                     *fixture.Pad, {});
  ReportBooleanBodyValidationFailures(*fixture.Part);
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
  EXPECT_FALSE(SpherePatchHasPoleCorner(*result.OutputBody))
      << "fixture rectangle pad ∩ sphere must be a lune, not a pole cusp";
}

TEST(UntitledXlBoolean, PadIntersectBuildDiagnostics)
{
  const CornerPadSphereFixture fixture = MakeCornerPadSphereFixture();
  if (!FixtureReady(fixture))
  {
    GTEST_SKIP() << "failed to build corner pad/sphere fixture";
  }

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result = eval->Evaluate(boolean::BooleanOp::Intersect,
                                     fixture.Part->Model(), *fixture.Sphere,
                                     *fixture.Pad, {});
  ReportBooleanBodyValidationFailures(*fixture.Part);
  EXPECT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

TEST(UntitledXlBoolean, PadIntersectFromXlFileIfAvailable)
{
  const char* xlPath = OptionalUntitledXlPath();
  if (xlPath == nullptr)
  {
    xlPath = "C:/Users/xingbl/Desktop/untitled.xl";
  }

  const auto loaded = io::LoadXl(xlPath);
  if (!loaded.Ok())
  {
    GTEST_SKIP() << "xl not loadable: " << loaded.Error;
  }

  Part* part = loaded.document->MainPart();
  ASSERT_NE(part, nullptr);

  Body* pad = FindBodyByGuid(*part, "0da825c1-d12b-4cd1-9a79-dbaea59ad4df");
  Body* sphere = FindBodyByGuid(*part, "3011fb97-f2d8-4f54-86ee-8a686dce3131");
  if (pad == nullptr || sphere == nullptr)
  {
    GTEST_SKIP() << "expected pad/sphere guids not in xl";
  }

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, part->Model(), *sphere, *pad, {});
  ReportBooleanBodyValidationFailures(*part);
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());

  bool sawTop = false;
  bool sawSpherePatch = false;
  for (Shell* shell : result.OutputBody->Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (face == nullptr)
      {
        continue;
      }
      TriangleMesh mesh;
      TessellateFace(*face, mesh);
      const std::size_t triangles = mesh.Indices.size() / 3;
      EXPECT_GE(triangles, 1u) << "face '" << face->Name << "' has no fill";
      if (face->Name.find("Pad_top") != std::string::npos)
      {
        sawTop = true;
      }
      if (face->Name.find("sphere_imprint_patch") != std::string::npos)
      {
        sawSpherePatch = true;
      }
    }
  }
  EXPECT_TRUE(sawTop);
  EXPECT_TRUE(sawSpherePatch);
  EXPECT_FALSE(SpherePatchHasPoleCorner(*result.OutputBody))
      << "sphere patch must not pinch at an analytic pole";
}

void DumpBodySphereLoops(const Body& body)
{
  for (Shell* shell : body.Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (face == nullptr || face->Surface == nullptr ||
          face->Surface->Kind() != SurfaceKind::Sphere)
      {
        continue;
      }
      const auto* sphere = static_cast<const SphereSurface*>(face->Surface);
      std::cout << "SPHERE FACE '" << face->Name << "' r=" << sphere->Radius()
                << " center=" << sphere->Center() << " loops="
                << face->Loops.size() << "\n";
      for (const Loop* loop : face->Loops)
      {
        if (loop == nullptr)
        {
          continue;
        }
        std::cout << "  LOOP '" << loop->Name << "' n=" << loop->CoedgeCount()
                  << "\n";
        loop->ForEachCoedge([&](const CoEdge& coedge)
        {
          const Edge* edge = coedge.Edge;
          if (edge == nullptr || edge->Curve == nullptr)
          {
            std::cout << "    (null edge)\n";
            return;
          }
          const Point3d from = coedge.From() != nullptr
                                   ? coedge.From()->Position()
                                   : Point3d{};
          const Point3d to =
              coedge.To() != nullptr ? coedge.To()->Position() : Point3d{};
          const Point3d eval0 = edge->Curve->Eval(edge->T0);
          const Point3d eval1 = edge->Curve->Eval(edge->T1);
          double chord = from.distance_to(to);
          std::cout << "    edge='" << edge->Name << "' kind="
                    << static_cast<int>(edge->Curve->Kind()) << " T0="
                    << edge->T0 << " T1=" << edge->T1 << " chord=" << chord
                    << "\n      from=" << from << " to=" << to
                    << "\n      evalT0=" << eval0 << " dV0="
                    << eval0.distance_to(edge->V0 ? edge->V0->Position()
                                                  : Point3d{})
                    << " evalT1=" << eval1 << " dV1="
                    << eval1.distance_to(edge->V1 ? edge->V1->Position()
                                                  : Point3d{})
                    << "\n";
          if (const auto* circle =
                  dynamic_cast<const CircleCurve*>(edge->Curve))
          {
            std::cout << "      circle c=" << circle->Center()
                      << " r=" << circle->Radius()
                      << " n=" << circle->Normal() << "\n";
          }
        });
      }
    }
  }
}

TEST(UntitledXlBoolean, FixtureIntersectionCirclesDump)
{
  const CornerPadSphereFixture fixture = MakeCornerPadSphereFixture();
  if (!FixtureReady(fixture))
  {
    GTEST_SKIP();
  }
  boolean::PipelineState state;
  state.Op = boolean::BooleanOp::Intersect;
  state.BodyA = fixture.Sphere;
  state.BodyB = fixture.Pad;
  state.TargetModel = &fixture.Part->Model();
  boolean::TopologyCopyContext ctxA{fixture.Part->Model()};
  state.WorkingBodyA = boolean::CopyBodySubgraph(ctxA, *fixture.Sphere, "_dumpA");
  boolean::TopologyCopyContext ctxB{fixture.Part->Model()};
  state.WorkingBodyB = boolean::CopyBodySubgraph(ctxB, *fixture.Pad, "_dumpB");
  state.IntersectionGraph = boolean::BuildIntersectionGraph(
      *state.WorkingBodyA, *state.WorkingBodyB);
  std::cout << "CIRCLES " << state.IntersectionGraph.Circles.size() << "\n";
  for (const auto& circle : state.IntersectionGraph.Circles)
  {
    const char* fa =
        circle.FaceA != nullptr ? circle.FaceA->Name.c_str() : "(null)";
    const char* fb =
        circle.FaceB != nullptr ? circle.FaceB->Name.c_str() : "(null)";
    std::cout << "  circle n=" << circle.Normal << " r=" << circle.Radius
              << " closed=" << circle.Closed << " A=" << fa << " B=" << fb
              << " c=" << circle.Center << "\n";
  }
  std::cout << "SEGMENTS " << state.IntersectionGraph.Segments.size() << "\n";
  for (const auto& segment : state.IntersectionGraph.Segments)
  {
    if (segment.CurveKind != CurveKind::Circle)
    {
      continue;
    }
    const char* fa =
        segment.FaceA != nullptr ? segment.FaceA->Name.c_str() : "(null)";
    const char* fb =
        segment.FaceB != nullptr ? segment.FaceB->Name.c_str() : "(null)";
    std::cout << "  seg " << fa << " / " << fb << " " << segment.Start << " -> "
              << segment.End << "\n";
  }
}

TEST(UntitledXlBoolean, ResultGuidSphereCuspDump)
{
  const char* xlPath = OptionalUntitledXlPath();
  if (xlPath == nullptr)
  {
    xlPath = "C:/Users/xingbl/Desktop/untitled.xl";
  }
  const auto loaded = io::LoadXl(xlPath);
  if (!loaded.Ok())
  {
    GTEST_SKIP() << "xl not loadable";
  }
  Part* part = loaded.document->MainPart();
  ASSERT_NE(part, nullptr);

  Body* pad = nullptr;
  Body* sphere = nullptr;
  for (const auto& body : part->Model().Bodies())
  {
    if (body == nullptr)
    {
      continue;
    }
    if (pad == nullptr && body->Name.find("Pad") != std::string::npos &&
        body->Name.find("bool_") == std::string::npos)
    {
      pad = body.get();
    }
    if (sphere == nullptr && body->Name.find("sphere") != std::string::npos &&
        body->Name.find("bool_") == std::string::npos)
    {
      sphere = body.get();
    }
  }
  Body* result = FindBodyByGuid(*part, "66cc7364-f36e-4778-ae1b-b3e75083291e");
  if (pad != nullptr && sphere != nullptr)
  {
    boolean::PipelineState state;
    state.Op = boolean::BooleanOp::Intersect;
    state.BodyA = sphere;
    state.BodyB = pad;
    state.TargetModel = &part->Model();
    boolean::TopologyCopyContext ctxA{part->Model()};
    state.WorkingBodyA = boolean::CopyBodySubgraph(ctxA, *sphere, "_dumpA");
    boolean::TopologyCopyContext ctxB{part->Model()};
    state.WorkingBodyB = boolean::CopyBodySubgraph(ctxB, *pad, "_dumpB");
    state.IntersectionGraph = boolean::BuildIntersectionGraph(
        *state.WorkingBodyA, *state.WorkingBodyB);
    std::cout << "CIRCLES " << state.IntersectionGraph.Circles.size() << "\n";
    for (const auto& circle : state.IntersectionGraph.Circles)
    {
      const char* fa =
          circle.FaceA != nullptr ? circle.FaceA->Name.c_str() : "(null)";
      const char* fb =
          circle.FaceB != nullptr ? circle.FaceB->Name.c_str() : "(null)";
      std::cout << "  circle n=" << circle.Normal << " r=" << circle.Radius
                << " closed=" << circle.Closed << " A=" << fa << " B=" << fb
                << " c=" << circle.Center << "\n";
    }
    std::cout << "SEGMENTS " << state.IntersectionGraph.Segments.size() << "\n";
    for (const auto& segment : state.IntersectionGraph.Segments)
    {
      if (segment.CurveKind != CurveKind::Circle)
      {
        continue;
      }
      const char* fa =
          segment.FaceA != nullptr ? segment.FaceA->Name.c_str() : "(null)";
      const char* fb =
          segment.FaceB != nullptr ? segment.FaceB->Name.c_str() : "(null)";
      std::cout << "  seg " << fa << " / " << fb << " " << segment.Start
                << " -> " << segment.End << "\n";
    }
    part->Model().RemoveBody(state.WorkingBodyA->Guid);
    part->Model().RemoveBody(state.WorkingBodyB->Guid);
  }
  if (result == nullptr)
  {
    if (pad == nullptr || sphere == nullptr)
    {
      GTEST_SKIP() << "neither result guid nor pad/sphere in xl";
    }
    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto built =
        eval->Evaluate(boolean::BooleanOp::Intersect, part->Model(), *sphere,
                       *pad, {});
    ASSERT_TRUE(built.Ok()) << built.Diagnostics;
    result = built.OutputBody;
  }
  ASSERT_NE(result, nullptr);
  std::cout << "RESULT name='" << result->Name << "' guid="
            << result->Guid.ToString() << "\n";
  for (Shell* shell : result->Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (face != nullptr)
      {
        DumpFaceLoopCoedges(*face);
      }
    }
  }
  DumpBodySphereLoops(*result);
  // Pad_Sketch vertex (4.03792, 7.01871) sits on the sphere center, so the
  // two side planes are great circles that meet at the south pole. That
  // corner is the modeled solid, not a false imprint hit.
  EXPECT_TRUE(ValidateBody(*result).Ok());
}

TEST(UntitledXlBoolean, XlSelectionDiagnostics)
{
  const char* xlPath = OptionalUntitledXlPath();
  if (xlPath == nullptr)
  {
    xlPath = "C:/Users/xingbl/Desktop/untitled.xl";
  }
  const auto loaded = io::LoadXl(xlPath);
  if (!loaded.Ok())
  {
    GTEST_SKIP() << "xl not loadable";
  }
  Part* part = loaded.document->MainPart();
  Body* pad = FindBodyByGuid(*part, "0da825c1-d12b-4cd1-9a79-dbaea59ad4df");
  Body* sphere = FindBodyByGuid(*part, "3011fb97-f2d8-4f54-86ee-8a686dce3131");
  if (pad == nullptr || sphere == nullptr)
  {
    GTEST_SKIP() << "guids missing";
  }

  Model& model = part->Model();
  boolean::PipelineState state;
  state.Op = boolean::BooleanOp::Intersect;
  state.BodyA = sphere;
  state.BodyB = pad;
  state.TargetModel = &model;

  boolean::TopologyCopyContext ctxA{model};
  state.WorkingBodyA = boolean::CopyBodySubgraph(ctxA, *sphere, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  state.WorkingBodyB = boolean::CopyBodySubgraph(ctxB, *pad, "_wkB");
  ASSERT_NE(state.WorkingBodyA, nullptr);
  ASSERT_NE(state.WorkingBodyB, nullptr);

  state.IntersectionGraph =
      boolean::BuildIntersectionGraph(*state.WorkingBodyA, *state.WorkingBodyB);
  const boolean::ImprintResult imprint = boolean::RunTopologicalImprint(state);
  ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;

  for (Shell* shell : state.WorkingBodyB->Shells)
  {
    for (Face* face : shell->Faces)
    {
      if (face != nullptr && face->Name.find("_split") != std::string::npos)
      {
        DumpFaceLoopCoedges(*face);
      }
    }
  }
  DumpEdgeRadial(*state.WorkingBodyB);
  const ValidationReport padReport = ValidateBody(*state.WorkingBodyB);
  std::cout << "Pad_wkB ValidateBody ok=" << padReport.Ok()
            << " issues=" << padReport.Issues.size() << "\n";
  for (const ValidationIssue& issue : padReport.Issues)
  {
    if (issue.Severity == ValidationIssue::IssueSeverity::Error)
    {
      std::cout << "  " << issue.Where << ": " << issue.Message << "\n";
    }
  }

  std::cout << "sphere faces after imprint:\n";
  for (Shell* shell : state.WorkingBodyA->Shells)
  {
    for (Face* face : shell->Faces)
    {
      std::cout << "  " << face->Name << "\n";
    }
  }

  const double eps = 1e-7;
  const Body& solidA = *state.WorkingBodyA;
  const Body& solidB = *state.WorkingBodyB;
  for (Shell* shell : solidB.Shells)
  {
    for (Face* face : shell->Faces)
    {
      const boolean::FaceRegion region = boolean::ClassifyFaceAgainstBody(
          *face, solidA, solidB, eps, boolean::BooleanOp::Intersect);
      if (region == boolean::FaceRegion::In || region == boolean::FaceRegion::On)
      {
        std::cout << "SELECT B: " << face->Name << " region="
                  << static_cast<int>(region) << "\n";
      }
    }
  }
  for (Shell* shell : solidA.Shells)
  {
    for (Face* face : shell->Faces)
    {
      const boolean::FaceRegion region = boolean::ClassifyFaceAgainstBody(
          *face, solidB, solidA, eps, boolean::BooleanOp::Intersect);
      std::cout << "A: " << face->Name << " region=" << static_cast<int>(region)
                << "\n";
      if (region == boolean::FaceRegion::In || region == boolean::FaceRegion::On)
      {
        std::cout << "SELECT A: " << face->Name << " region="
                  << static_cast<int>(region) << "\n";
      }
    }
  }

  state.AVsB.clear();
  state.BVsA.clear();
  for (Shell* shell : solidA.Shells)
  {
    for (Face* face : shell->Faces)
    {
      state.AVsB.push_back({face, boolean::ClassifyFaceAgainstBody(
                                       *face, solidB, solidA, eps,
                                       boolean::BooleanOp::Intersect)});
    }
  }
  for (Shell* shell : solidB.Shells)
  {
    for (Face* face : shell->Faces)
    {
      state.BVsA.push_back({face, boolean::ClassifyFaceAgainstBody(
                                       *face, solidA, solidB, eps,
                                       boolean::BooleanOp::Intersect)});
    }
  }
  state.Selection =
      boolean::SelectCsgFaces(state.Op, state.AVsB, state.BVsA);
  const boolean::SeamExpansionReport seam =
      boolean::ReportSeamExpansion(state.Selection);
  std::cout << "seam unselected=" << seam.PartnerUnselected
            << " expanded=" << seam.Expanded
            << " fallback=" << seam.CopiedImprintFallback << "\n";
  for (const std::string& name : seam.FallbackImprintNames)
  {
    std::cout << "  fallback: " << name << "\n";
  }

  model.RemoveBody(state.WorkingBodyA->Guid);
  model.RemoveBody(state.WorkingBodyB->Guid);
}

}  // namespace brep
