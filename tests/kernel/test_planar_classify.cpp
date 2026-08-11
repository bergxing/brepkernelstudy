#include "api/core.hpp"
#include "api/modeling.hpp"

#include "brep/bool/classify.hpp"
#include "brep/bool/planar_recognize.hpp"
#include "brep/ops/profile.hpp"

#include <gtest/gtest.h>

namespace brep {
namespace {

TEST(PlanarClassify, BoxInOutOn) {
  BoxSpec box{.min = {0, 0, 0}, .max = {2, 2, 2}};
  constexpr double eps = 1e-7;
  EXPECT_EQ(boolean::classify_point_in_box(box, {1, 1, 1}, eps),
            boolean::SolidClass::In);
  EXPECT_EQ(boolean::classify_point_in_box(box, {3, 1, 1}, eps),
            boolean::SolidClass::Out);
  EXPECT_EQ(boolean::classify_point_in_box(box, {0, 1, 1}, eps),
            boolean::SolidClass::On);
}

TEST(PlanarClassify, LPrismInOut) {
  Model model;
  ops::ExtrudeSpec spec;
  spec.name = "L";
  spec.distance = 2.0;
  spec.plane = Plane::xz_y_up();
  spec.profile.outer = {
      Point2d{0, 0}, Point2d{3, 0}, Point2d{3, 1},
      Point2d{1, 1}, Point2d{1, 2}, Point2d{0, 2},
  };
  Body* body = ops::extrude(model, spec);
  ASSERT_NE(body, nullptr);
  auto prism = boolean::recognize_extrusion_prism(*body, {});
  ASSERT_TRUE(prism.has_value());

  constexpr double eps = 1e-7;
  // Inside long arm of L (x=2, z=0.5, y=1)
  EXPECT_EQ(boolean::classify_point_in_prism(*prism, {2, 1, 0.5}, eps),
            boolean::SolidClass::In);
  // In the notch (outside L): x=2, z=1.5
  EXPECT_EQ(boolean::classify_point_in_prism(*prism, {2, 1, 1.5}, eps),
            boolean::SolidClass::Out);
  // Above extrusion
  EXPECT_EQ(boolean::classify_point_in_prism(*prism, {0.5, 3, 0.5}, eps),
            boolean::SolidClass::Out);
}

}  // namespace
}  // namespace brep
