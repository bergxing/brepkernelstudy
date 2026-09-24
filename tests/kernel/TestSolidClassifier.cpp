#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/SolidClassifier.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(SolidClassifier, BoxBodyInteriorPointIsInside)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}});
  ASSERT_NE(box, nullptr);
  EXPECT_EQ(boolean::ClassifyPointInBody(*box, {1, 1, 1}, 1e-6),
            boolean::SolidClass::In);
  EXPECT_EQ(boolean::ClassifyPointInBody(*box, {3, 1, 1}, 1e-6),
            boolean::SolidClass::Out);
  EXPECT_EQ(boolean::ClassifyPointInBody(*box, {1, 1, 0}, 1e-6),
            boolean::SolidClass::On);
}

TEST(SolidClassifier, SphereBodyInteriorPointIsInside)
{
  Model model;
  Body* sphere = MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0});
  ASSERT_NE(sphere, nullptr);
  EXPECT_EQ(boolean::ClassifyPointInBody(*sphere, {0, 0, 0}, 1e-6),
            boolean::SolidClass::In);
  EXPECT_EQ(boolean::ClassifyPointInBody(*sphere, {2, 0, 0}, 1e-6),
            boolean::SolidClass::Out);
  EXPECT_EQ(boolean::ClassifyPointInBody(*sphere, {1, 0, 0}, 1e-6),
            boolean::SolidClass::On);
}

}  // namespace
}  // namespace brep
