#include "api/Core.h"


#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"


#include "brep/bool/Boolean.h"


#include "brep/bool/CompositeEvaluator.h"


#include "brep/bool/FaceSelector.h"


#include "brep/bool/Pipeline.h"


#include "brep/Validate.h"


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


TEST(CompositeEvaluator, BoxBoxUsesGeneralPipeline)


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


  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);


}


TEST(BooleanPipeline, BoxBoxSixStageGeneralPipeline)


{


  Model model;


  Body* a = MakeBox(


      model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "A"});


  Body* b = MakeBox(


      model, BoxSpec{.Min = {1, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});


  ASSERT_NE(a, nullptr);


  ASSERT_NE(b, nullptr);


  auto pipeline = boolean::MakeDefaultBooleanPipeline();


  boolean::BooleanContext ctx;


  const auto result =


      pipeline->Evaluate(boolean::BooleanOp::Union, model, *a, *b, ctx);


  EXPECT_TRUE(result.Ok()) << result.Diagnostics;


  EXPECT_EQ(result.Mode, boolean::BooleanEvalMode::General);


  ASSERT_NE(result.OutputBody, nullptr);


  EXPECT_TRUE(ValidateBody(*result.OutputBody).Ok());


}




TEST(ImprintEngine, SphereBoxSubtractUsesCurvedImprint)


{


  Model model;


  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}});


  Body* sphere = MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1});


  ASSERT_NE(box, nullptr);


  ASSERT_NE(sphere, nullptr);


  auto eval = boolean::MakeDefaultBooleanEvaluator();


  const auto sphereMinusBox =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});


  ASSERT_TRUE(sphereMinusBox.Ok()) << sphereMinusBox.Diagnostics;
  ASSERT_NE(sphereMinusBox.OutputBody, nullptr);
  EXPECT_EQ(sphereMinusBox.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*sphereMinusBox.OutputBody).Ok());


  const auto boxMinusSphere =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *box, *sphere, {});


  ASSERT_TRUE(boxMinusSphere.Ok()) << boxMinusSphere.Diagnostics;
  ASSERT_NE(boxMinusSphere.OutputBody, nullptr);
  EXPECT_EQ(boxMinusSphere.Mode, boolean::BooleanEvalMode::General);
  EXPECT_TRUE(ValidateBody(*boxMinusSphere.OutputBody).Ok());


}


}  // namespace


}  // namespace brep
