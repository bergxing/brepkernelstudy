#pragma once

#include "brep/bool/Types.h"
#include "brep/feat/PrimitiveSpecs.h"

#include <string>
#include <string_view>
#include <vector>

namespace brep
{

enum class PropertyKind
{
    Length,
    Real,
    Integer,
    Hint,
};

enum class PropertyWidget
{
    Spin,
    SliderSpin,
    ReadOnly,
};

struct PropertyField
{
    std::string Id;
    std::string Label;
    PropertyKind Kind{PropertyKind::Real};
    PropertyWidget Widget{PropertyWidget::Spin};
    double Value{0.0};
    double Min{1.0e-4};
    double Max{1.0e9};
    double Step{0.1};
};

struct PropertySheet
{
    std::string GroupTitle;
    std::string Hint;
    bool Editable{false};
    std::vector<PropertyField> Fields;
};

[[nodiscard]] PropertySheet Describe(const PrimitiveSpec& spec);
[[nodiscard]] PropertySheet DescribeBoolean(boolean::BooleanOp op);
[[nodiscard]] bool Apply(PrimitiveSpec& spec, std::string_view fieldId,
                         double value);

}  // namespace brep
