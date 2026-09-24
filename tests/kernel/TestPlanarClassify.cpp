#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/bool/Classify.h"
#include "brep/bool/PlanarRecognize.h"
#include "brep/ops/Profile.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(PlanarClassify, BoxInOutOn)
{
  BoxSpec box{.Min = {0, 0, 0}, .Max = {2, 2, 2}};
  constexpr double eps = 1e-7;
  EXPECT_EQ(boolean::ClassifyPointInBox(box, {1, 1, 1}, eps),
            boolean::SolidClass::In);
  EXPECT_EQ(boolean::ClassifyPointInBox(box, {3, 1, 1}, eps),
            boolean::SolidClass::Out);
  EXPECT_EQ(boolean::ClassifyPointInBox(box, {0, 1, 1}, eps),
            boolean::SolidClass::On);
}

TEST(PlanarClassify, LPrismInOut)
{
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "L";
  spec.Distance = 2.0;
  spec.Plane = Plane::XzYUp();
  spec.Profile.Outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* body = ops::Extrude(model, spec);
  ASSERT_NE(body, nullptr);
  auto prism = boolean::RecognizeExtrusionPrism(*body, {});
  ASSERT_TRUE(prism.has_value());

  constexpr double eps = 1e-7;
  // Inside long arm of L (x=2, z=0.5, y=1)
  EXPECT_EQ(boolean::ClassifyPointInPrism(*prism, {2, 1, 0.5}, eps),
            boolean::SolidClass::In);
  // In the notch (outside L): x=2, z=1.5
  EXPECT_EQ(boolean::ClassifyPointInPrism(*prism, {2, 1, 1.5}, eps),
            boolean::SolidClass::Out);
  // Above extrusion
  EXPECT_EQ(boolean::ClassifyPointInPrism(*prism, {0.5, 3, 0.5}, eps),
            boolean::SolidClass::Out);
}

}  // namespace
}  // namespace brep
