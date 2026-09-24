#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/ops/Profile.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

Body* MakeExtrudePad(Model& model)
{
  ops::ExtrudeSpec spec;
  spec.Name = "pad";
  spec.Plane = Plane::XzYUp();
  spec.Distance = 1.0;
  spec.Symmetric = false;
  spec.Profile.Outer = {
      Point2d{0, 0},
      Point2d{2, 0},
      Point2d{2, 2},
      Point2d{0, 2},
  };
  return ops::Extrude(model, spec);
}

void RunPadSphereUnion(bool padIsA)
{
  Model model;
  Body* pad = MakeExtrudePad(model);
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {1.0, 0.5, 1.0}, .Radius = 0.8, .Name = "sphere"});
  ASSERT_NE(pad, nullptr);
  ASSERT_NE(sphere, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result = padIsA
                          ? eval->Evaluate(boolean::BooleanOp::Union, model, *pad, *sphere, {})
                          : eval->Evaluate(boolean::BooleanOp::Union, model, *sphere, *pad, {});
  SCOPED_TRACE(padIsA ? "pad=A sphere=B" : "sphere=A pad=B");
  if (!result.Ok())
  {
    for (const auto& body : model.Bodies())
    {
      if (body && body->Name.rfind("bool_", 0) == 0)
      {
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
  }
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

TEST(PadSphereOrder, WorkingBodyAfterImprintValidates)
{
  Model model;
  Body* pad = MakeExtrudePad(model);
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {1.0, 0.5, 1.0}, .Radius = 0.8, .Name = "sphere"});
  ASSERT_NE(pad, nullptr);
  ASSERT_NE(sphere, nullptr);

  boolean::TopologyCopyContext ctxA{model};
  Body* workingPad = boolean::CopyBodySubgraph(ctxA, *pad, "_wkA");
  boolean::TopologyCopyContext ctxB{model};
  Body* workingSphere = boolean::CopyBodySubgraph(ctxB, *sphere, "_wkB");
  ASSERT_NE(workingPad, nullptr);
  ASSERT_NE(workingSphere, nullptr);

  boolean::PipelineState state;
  state.BodyA = pad;
  state.BodyB = sphere;
  state.WorkingBodyA = workingPad;
  state.WorkingBodyB = workingSphere;
  state.TargetModel = &model;
  state.IntersectionGraph =
      boolean::BuildIntersectionGraph(*workingPad, *workingSphere);
  const boolean::ImprintResult imprint = boolean::RunTopologicalImprint(state);
  ASSERT_TRUE(imprint.Ok) << imprint.Diagnostics;

  const ValidationReport padReport = ValidateBody(*workingPad);
  EXPECT_TRUE(padReport.Ok());
  EXPECT_TRUE(ValidateBody(*workingSphere).Ok());
}

TEST(PadSphereOrder, PadAsA)
{
  RunPadSphereUnion(true);
}

TEST(PadSphereOrder, SphereAsA)
{
  RunPadSphereUnion(false);
}

TEST(PadSphereOrder, IntersectSphereAsA)
{
  Model model;
  Body* pad = MakeExtrudePad(model);
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {1.0, 0.5, 1.0}, .Radius = 0.8, .Name = "sphere"});
  ASSERT_NE(pad, nullptr);
  ASSERT_NE(sphere, nullptr);

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *sphere, *pad, {});
  if (!result.Ok())
  {
    for (const auto& body : model.Bodies())
    {
      if (body && body->Name.rfind("bool_", 0) == 0)
      {
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
  }
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);
  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());
}

}  // namespace
}  // namespace brep
