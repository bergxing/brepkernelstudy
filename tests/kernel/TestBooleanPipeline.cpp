#include "api/Core.h"


#include "api/Modeling.h"


#include "brep/bool/Boolean.h"


#include "brep/bool/CompositeEvaluator.h"


#include "brep/bool/FaceSelector.h"


#include "brep/bool/FastPath.h"


#include "brep/bool/Pipeline.h"


#include <gtest/gtest.h>


#include <memory>


namespace brep


{


namespace


{


TEST(BooleanPipeline, DefaultStageOrder)


{


  auto pipeline = boolean::MakeDefaultBooleanPipeline();


  ASSERT_NE(pipeline, nullptr);


  const auto& order = pipeline->StageOrder();


  ASSERT_EQ(order.size(), 6u);


  EXPECT_EQ(order[0], boolean::PipelineStage::Preprocess);


  EXPECT_EQ(order[1], boolean::PipelineStage::Intersect);


  EXPECT_EQ(order[2], boolean::PipelineStage::Imprint);


  EXPECT_EQ(order[3], boolean::PipelineStage::Classify);


  EXPECT_EQ(order[4], boolean::PipelineStage::Select);


  EXPECT_EQ(order[5], boolean::PipelineStage::Build);


}


TEST(BooleanPipeline, StopsAtImprintForBoxSphere)


{


  Model model;


  Body* box = MakeBox(


      model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "B"});


  Body* sphere = MakeSphere(


      model, SphereSpec{.Center = {1, 1, 1}, .Radius = 0.75, .Name = "S"});


  ASSERT_NE(box, nullptr);


  ASSERT_NE(sphere, nullptr);


  auto pipeline = boolean::MakeDefaultBooleanPipeline();


  const auto result = pipeline->Evaluate(boolean::BooleanOp::Subtract, model,


                                         *box, *sphere, {});


  EXPECT_FALSE(result.Ok());


  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);


  EXPECT_NE(result.Diagnostics.find("Imprint"), std::string::npos)


      << result.Diagnostics;


}


TEST(FaceSelector, SubtractKeepsAOutAndBIn)


{


  Face fa{};


  Face fb{};


  const std::vector<boolean::FaceClassification> a_vs_b{


      {.TargetFace = &fa, .Region = boolean::FaceRegion::Out},


      {.TargetFace = &fa, .Region = boolean::FaceRegion::In},


  };


  const std::vector<boolean::FaceClassification> b_vs_a{


      {.TargetFace = &fb, .Region = boolean::FaceRegion::In},


      {.TargetFace = &fb, .Region = boolean::FaceRegion::Out},


  };


  const boolean::FaceSelection sel = boolean::SelectCsgFaces(


      boolean::BooleanOp::Subtract, a_vs_b, b_vs_a);


  EXPECT_EQ(sel.FromA.size(), 1u);


  EXPECT_EQ(sel.FromB.size(), 1u);


  EXPECT_TRUE(sel.ReverseB);


}


TEST(CompositeEvaluator, BoxBoxUsesAnalyticFastPath)


{


  Model model;


  Body* a = MakeBox(


      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});


  Body* b = MakeBox(


      model, BoxSpec{.Min = {0.5, 0.5, 0.5}, .Max = {1.5, 1.5, 1.5}, .Name = "B"});


  ASSERT_NE(a, nullptr);


  ASSERT_NE(b, nullptr);


  auto eval = boolean::MakeDefaultBooleanEvaluator();


  const auto result =


      eval->Evaluate(boolean::BooleanOp::Union, model, *a, *b, {});


  EXPECT_TRUE(result.Ok()) << result.Diagnostics;


}


TEST(CompositeEvaluator, BoxMinusSphereFallsThroughToPipeline)


{


  Model model;


  Body* box = MakeBox(


      model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "B"});


  Body* sphere = MakeSphere(


      model, SphereSpec{.Center = {1, 1, 1}, .Radius = 0.75, .Name = "S"});


  ASSERT_NE(box, nullptr);


  ASSERT_NE(sphere, nullptr);


  auto eval = boolean::MakeDefaultBooleanEvaluator();


  const auto result =


      eval->Evaluate(boolean::BooleanOp::Subtract, model, *box, *sphere, {});


  EXPECT_FALSE(result.Ok());


  EXPECT_NE(result.Diagnostics.find("Imprint"), std::string::npos)


      << result.Diagnostics;


}


TEST(FastPathRegistry, SphereBoxSubtractOnlyWhenSphereIsA)


{


  boolean::AnalyticFastPathRegistry registry =


      boolean::MakeDefaultFastPathRegistry();


  Model model;


  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});


  Body* sphere = MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1});


  ASSERT_NE(box, nullptr);


  ASSERT_NE(sphere, nullptr);


  const boolean::BooleanContext ctx;


  const boolean::BooleanOp sub = boolean::BooleanOp::Subtract;


  bool sphere_a_handles = false;


  bool box_a_handles = false;


  for (const auto& path : registry.Paths())


  {


    if (path->Name() != "SphereBox") continue;


    sphere_a_handles = path->CanHandle(sub, *sphere, *box, ctx);


    box_a_handles = path->CanHandle(sub, *box, *sphere, ctx);


    break;


  }


  EXPECT_TRUE(sphere_a_handles);


  EXPECT_FALSE(box_a_handles);


}


}  // namespace


}  // namespace brep
