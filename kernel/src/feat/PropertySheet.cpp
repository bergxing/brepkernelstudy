#include "brep/feat/PropertySheet.h"

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <type_traits>

namespace brep
{
namespace
{

constexpr std::string_view kLengthId = "length";
constexpr std::string_view kHeightId = "height";
constexpr std::string_view kWidthId = "width";
constexpr std::string_view kRadiusId = "radius";

constexpr const char* kDimGroupTitle = "Dimensions (parameters)";
constexpr const char* kWeightGroupTitle = "Weights";
constexpr const char* kParamHint =
    "Parameter-driven · edits regenerate the model";

[[nodiscard]] bool IsPositiveFinite(double value) noexcept
{
    return std::isfinite(value) && value > 0.0;
}

[[nodiscard]] PropertyField MakeLengthField(std::string id, std::string label,
                                            double value)
{
    PropertyField field;
    field.Id = std::move(id);
    field.Label = std::move(label);
    field.Kind = PropertyKind::Length;
    field.Widget = PropertyWidget::Spin;
    field.Value = value;
    field.Min = 1.0e-4;
    field.Max = 1.0e9;
    field.Step = 0.1;
    return field;
}

[[nodiscard]] PropertyField MakeWeightField(std::size_t index, double value)
{
    PropertyField field;
    field.Id = "w" + std::to_string(index);
    field.Label = field.Id;
    field.Kind = PropertyKind::Real;
    field.Widget = PropertyWidget::SliderSpin;
    field.Value = value;
    field.Min = 0.01;
    field.Max = 10.0;
    field.Step = 0.05;
    return field;
}

[[nodiscard]] PropertySheet DescribeBox(const BoxSpec& spec)
{
    PropertySheet sheet;
    sheet.GroupTitle = kDimGroupTitle;
    sheet.Hint = kParamHint;
    sheet.Editable = true;
    sheet.Fields.push_back(
        MakeLengthField(std::string(kLengthId), "Length (X)",
                        spec.Max.x() - spec.Min.x()));
    sheet.Fields.push_back(
        MakeLengthField(std::string(kHeightId), "Height (Y)",
                        spec.Max.y() - spec.Min.y()));
    sheet.Fields.push_back(
        MakeLengthField(std::string(kWidthId), "Width (Z)",
                        spec.Max.z() - spec.Min.z()));
    return sheet;
}

[[nodiscard]] PropertySheet DescribeSphere(const SphereSpec& spec)
{
    PropertySheet sheet;
    sheet.GroupTitle = kDimGroupTitle;
    sheet.Hint = kParamHint;
    sheet.Editable = true;
    sheet.Fields.push_back(
        MakeLengthField(std::string(kRadiusId), "Radius", spec.Radius));
    return sheet;
}

[[nodiscard]] PropertySheet DescribeBezier(const BezierSpec& spec)
{
    PropertySheet sheet;
    sheet.GroupTitle = kWeightGroupTitle;
    sheet.Hint = kParamHint;
    sheet.Editable = true;
    for (std::size_t i = 0; i < spec.Cvs.size(); ++i)
    {
        sheet.Fields.push_back(MakeWeightField(i, BezierWeightAt(spec, i)));
    }
    return sheet;
}

[[nodiscard]] PropertySheet DescribeNurbs(const NurbsCurveSpec& spec)
{
    PropertySheet sheet;
    sheet.GroupTitle = kWeightGroupTitle;
    sheet.Hint = kParamHint;
    sheet.Editable = true;
    for (std::size_t i = 0; i < spec.Cvs.size(); ++i)
    {
        sheet.Fields.push_back(MakeWeightField(i, NurbsWeightAt(spec, i)));
    }
    return sheet;
}

[[nodiscard]] std::optional<std::size_t> ParseWeightIndex(
    std::string_view fieldId)
{
    if (fieldId.size() < 2 || fieldId.front() != 'w')
    {
        return std::nullopt;
    }
    const std::string_view digits = fieldId.substr(1);
    std::size_t index = 0;
    const auto parsed = std::from_chars(
        digits.data(), digits.data() + digits.size(), index);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != digits.data() + digits.size())
    {
        return std::nullopt;
    }
    return index;
}

[[nodiscard]] bool ApplyBox(BoxSpec& spec, std::string_view fieldId,
                            double value)
{
    if (!IsPositiveFinite(value))
    {
        return false;
    }
    if (fieldId == kLengthId)
    {
        spec.Max = Point3d{spec.Min.x() + value, spec.Max.y(), spec.Max.z()};
        return true;
    }
    if (fieldId == kHeightId)
    {
        spec.Max = Point3d{spec.Max.x(), spec.Min.y() + value, spec.Max.z()};
        return true;
    }
    if (fieldId == kWidthId)
    {
        spec.Max = Point3d{spec.Max.x(), spec.Max.y(), spec.Min.z() + value};
        return true;
    }
    return false;
}

[[nodiscard]] bool ApplySphere(SphereSpec& spec, std::string_view fieldId,
                               double value)
{
    if (fieldId != kRadiusId || !IsPositiveFinite(value))
    {
        return false;
    }
    spec.Radius = value;
    return true;
}

[[nodiscard]] bool ApplyBezier(BezierSpec& spec, std::string_view fieldId,
                               double value)
{
    if (!IsPositiveFinite(value))
    {
        return false;
    }
    const auto index = ParseWeightIndex(fieldId);
    if (!index.has_value() || *index >= spec.Cvs.size())
    {
        return false;
    }
    if (spec.Weights.size() < spec.Cvs.size())
    {
        spec.Weights.resize(spec.Cvs.size(), 1.0);
    }
    spec.Weights[*index] = value;
    return true;
}

[[nodiscard]] bool ApplyNurbs(NurbsCurveSpec& spec, std::string_view fieldId,
                              double value)
{
    if (!IsPositiveFinite(value))
    {
        return false;
    }
    const auto index = ParseWeightIndex(fieldId);
    if (!index.has_value() || *index >= spec.Cvs.size())
    {
        return false;
    }
    if (spec.Weights.size() < spec.Cvs.size())
    {
        spec.Weights.resize(spec.Cvs.size(), 1.0);
    }
    spec.Weights[*index] = value;
    return true;
}

}  // namespace

PropertySheet Describe(const PrimitiveSpec& spec)
{
    return std::visit(
        [](const auto& value) -> PropertySheet
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
            {
                return DescribeBox(value);
            }
            else if constexpr (std::is_same_v<T, SphereSpec>)
            {
                return DescribeSphere(value);
            }
            else if constexpr (std::is_same_v<T, BezierSpec>)
            {
                return DescribeBezier(value);
            }
            else
            {
                static_assert(std::is_same_v<T, NurbsCurveSpec>,
                              "Describe: add PrimitiveSpec arm");
                return DescribeNurbs(value);
            }
        },
        spec);
}

PropertySheet DescribeBoolean(boolean::BooleanOp op)
{
    PropertySheet sheet;
    sheet.Editable = false;
    switch (op)
    {
    case boolean::BooleanOp::Union:
        sheet.Hint = "Operation: Union (Fuse)";
        break;
    case boolean::BooleanOp::Subtract:
        sheet.Hint = "Operation: Subtract (Cut)";
        break;
    case boolean::BooleanOp::Intersect:
        sheet.Hint = "Operation: Intersect (Common)";
        break;
    }
    return sheet;
}

bool Apply(PrimitiveSpec& spec, std::string_view fieldId, double value)
{
    return std::visit(
        [fieldId, value](auto& arm) -> bool
        {
            using T = std::decay_t<decltype(arm)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
            {
                return ApplyBox(arm, fieldId, value);
            }
            else if constexpr (std::is_same_v<T, SphereSpec>)
            {
                return ApplySphere(arm, fieldId, value);
            }
            else if constexpr (std::is_same_v<T, BezierSpec>)
            {
                return ApplyBezier(arm, fieldId, value);
            }
            else
            {
                static_assert(std::is_same_v<T, NurbsCurveSpec>,
                              "Apply: add PrimitiveSpec arm");
                return ApplyNurbs(arm, fieldId, value);
            }
        },
        spec);
}

}  // namespace brep
