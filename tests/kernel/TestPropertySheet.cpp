#include "api/Core.h"
#include "api/Modeling.h"

#include <gtest/gtest.h>

#include <string>
#include <variant>
#include <vector>

namespace brep
{
namespace
{

[[nodiscard]] const PropertyField* FindField(const PropertySheet& sheet,
                                             std::string_view id)
{
    for (const PropertyField& field : sheet.Fields)
    {
        if (field.Id == id)
        {
            return &field;
        }
    }
    return nullptr;
}

TEST(PropertySheet, DescribeBoxHasXyzLengths)
{
    const BoxSpec box{.Min = {1, 2, 3}, .Max = {4, 6, 8}};
    const PropertySheet sheet = Describe(PrimitiveSpec{box});
    ASSERT_EQ(sheet.Fields.size(), 3u);
    EXPECT_TRUE(sheet.Editable);
    EXPECT_EQ(sheet.GroupTitle, "Dimensions (parameters)");
    ASSERT_NE(FindField(sheet, "length"), nullptr);
    ASSERT_NE(FindField(sheet, "height"), nullptr);
    ASSERT_NE(FindField(sheet, "width"), nullptr);
    EXPECT_NEAR(FindField(sheet, "length")->Value, 3.0, 1e-12);
    EXPECT_NEAR(FindField(sheet, "height")->Value, 4.0, 1e-12);
    EXPECT_NEAR(FindField(sheet, "width")->Value, 5.0, 1e-12);
    EXPECT_EQ(FindField(sheet, "length")->Label, "Length (X)");
    EXPECT_EQ(FindField(sheet, "height")->Label, "Height (Y)");
    EXPECT_EQ(FindField(sheet, "width")->Label, "Width (Z)");
}

TEST(PropertySheet, ApplyBoxLengthKeepsMin)
{
    PrimitiveSpec spec{BoxSpec{.Min = {1, 2, 3}, .Max = {4, 6, 8}}};
    ASSERT_TRUE(Apply(spec, "length", 2.0));
    const auto* box = std::get_if<BoxSpec>(&spec);
    ASSERT_NE(box, nullptr);
    EXPECT_NEAR(box->Min.x(), 1.0, 1e-12);
    EXPECT_NEAR(box->Min.y(), 2.0, 1e-12);
    EXPECT_NEAR(box->Min.z(), 3.0, 1e-12);
    EXPECT_NEAR(box->Max.x() - box->Min.x(), 2.0, 1e-12);
    EXPECT_NEAR(box->Max.y(), 6.0, 1e-12);
    EXPECT_NEAR(box->Max.z(), 8.0, 1e-12);
}

TEST(PropertySheet, DescribeAndApplySphereRadius)
{
    PrimitiveSpec spec{SphereSpec{.Center = {0, 0, 0}, .Radius = 1.5}};
    const PropertySheet sheet = Describe(spec);
    ASSERT_EQ(sheet.Fields.size(), 1u);
    EXPECT_EQ(sheet.Fields[0].Id, "radius");
    EXPECT_EQ(sheet.Fields[0].Label, "Radius");
    EXPECT_NEAR(sheet.Fields[0].Value, 1.5, 1e-12);
    ASSERT_TRUE(Apply(spec, "radius", 3.25));
    const auto* sphere = std::get_if<SphereSpec>(&spec);
    ASSERT_NE(sphere, nullptr);
    EXPECT_NEAR(sphere->Radius, 3.25, 1e-12);
}

TEST(PropertySheet, DescribeBezierTwoCvSliderSpin)
{
    const BezierSpec bezier{.Cvs = {{0, 0, 0}, {1, 0, 0}}};
    const PropertySheet sheet = Describe(PrimitiveSpec{bezier});
    ASSERT_EQ(sheet.Fields.size(), 2u);
    EXPECT_EQ(sheet.GroupTitle, "Weights");
    EXPECT_EQ(sheet.Fields[0].Id, "w0");
    EXPECT_EQ(sheet.Fields[1].Id, "w1");
    EXPECT_EQ(sheet.Fields[0].Widget, PropertyWidget::SliderSpin);
    EXPECT_EQ(sheet.Fields[1].Widget, PropertyWidget::SliderSpin);
    EXPECT_NEAR(sheet.Fields[0].Value, 1.0, 1e-12);
    EXPECT_NEAR(sheet.Fields[1].Value, 1.0, 1e-12);
}

TEST(PropertySheet, ApplyBezierWeightW1)
{
    PrimitiveSpec spec{BezierSpec{.Cvs = {{0, 0, 0}, {1, 0, 0}}}};
    ASSERT_TRUE(Apply(spec, "w1", 2.0));
    const auto* bezier = std::get_if<BezierSpec>(&spec);
    ASSERT_NE(bezier, nullptr);
    ASSERT_EQ(bezier->Weights.size(), 2u);
    EXPECT_NEAR(bezier->Weights[0], 1.0, 1e-12);
    EXPECT_NEAR(bezier->Weights[1], 2.0, 1e-12);
}

TEST(PropertySheet, DescribeAndApplyNurbsWeights)
{
    const NurbsCurveSpec nurbs{
        .Cvs = {{0, 0, 0}, {1, 0, 0}, {2, 1, 0}, {3, 1, 0}, {4, 0, 0}},
    };
    const PropertySheet sheet = Describe(PrimitiveSpec{nurbs});
    ASSERT_EQ(sheet.Fields.size(), 5u);
    EXPECT_EQ(sheet.GroupTitle, "Weights");
    EXPECT_EQ(sheet.Fields[0].Id, "w0");
    EXPECT_EQ(sheet.Fields[1].Id, "w1");
    EXPECT_EQ(sheet.Fields[2].Id, "w2");
    EXPECT_EQ(sheet.Fields[3].Id, "w3");
    EXPECT_EQ(sheet.Fields[4].Id, "w4");
    for (const PropertyField& field : sheet.Fields)
    {
        EXPECT_EQ(field.Widget, PropertyWidget::SliderSpin);
        EXPECT_NEAR(field.Value, 1.0, 1e-12);
    }

    PrimitiveSpec spec{nurbs};
    ASSERT_TRUE(Apply(spec, "w2", 2.0));
    const auto* after = std::get_if<NurbsCurveSpec>(&spec);
    ASSERT_NE(after, nullptr);
    ASSERT_EQ(after->Weights.size(), 5u);
    EXPECT_NEAR(after->Weights[2], 2.0, 1e-12);

    const std::vector<double> weightsBeforeReject = after->Weights;
    EXPECT_FALSE(Apply(spec, "w2", 0.0));
    ASSERT_EQ(std::get<NurbsCurveSpec>(spec).Weights, weightsBeforeReject);

    EXPECT_FALSE(Apply(spec, "w9", 1.0));
    ASSERT_EQ(std::get<NurbsCurveSpec>(spec).Weights, weightsBeforeReject);
}

TEST(PropertySheet, ApplyUnknownIdLeavesSpecUnchanged)
{
    PrimitiveSpec spec{BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}}};
    const BoxSpec before = std::get<BoxSpec>(spec);
    EXPECT_FALSE(Apply(spec, "nope", 2.0));
    const auto* box = std::get_if<BoxSpec>(&spec);
    ASSERT_NE(box, nullptr);
    EXPECT_NEAR(box->Min.x(), before.Min.x(), 1e-12);
    EXPECT_NEAR(box->Max.x(), before.Max.x(), 1e-12);
    EXPECT_NEAR(box->Max.y(), before.Max.y(), 1e-12);
    EXPECT_NEAR(box->Max.z(), before.Max.z(), 1e-12);
}

TEST(PropertySheet, DescribeBooleanSubtractHintOnly)
{
    const PropertySheet sheet = DescribeBoolean(boolean::BooleanOp::Subtract);
    EXPECT_FALSE(sheet.Editable);
    EXPECT_TRUE(sheet.Fields.empty());
    EXPECT_FALSE(sheet.Hint.empty());
    EXPECT_NE(sheet.Hint.find("Subtract"), std::string::npos);
}

}  // namespace
}  // namespace brep
