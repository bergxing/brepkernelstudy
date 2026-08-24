#pragma once

#include "brep/bool/Types.h"
#include "brep/Builder.h"
#include "brep/feat/Feature.h"
#include "brep/Math.h"
#include "brep/param/Parameter.h"

#include <string>
#include <utility>
#include <vector>

namespace brep
{
class Part;
}

namespace brep::feat
{

enum class TxKind
{
    AppendFeature,
    RemoveFeature,
    EditParameters,
    SuppressFeature,
    UnsuppressFeature,
};

struct FeatureTransaction
{
    TxKind Kind{TxKind::AppendFeature};
    FeatureId Feature{};
    std::string FeatureType;  // "Box", "Sphere", "Sketch", "Extrude", "Boolean"
    BoxSpec Box{};
    SphereSpec Sphere{};
    Point3d BoxOrigin{};
    std::string SketchName;
    FeatureId SketchFeatureId{};  // for Extrude upstream
    double ExtrudeDistance{1.0};
    boolean::BooleanOp Op{boolean::BooleanOp::Union};
    FeatureId TargetFeatureId{};
    FeatureId ToolFeatureId{};
    bool TargetWasSuppressed{false};
    bool ToolWasSuppressed{false};
    std::vector<std::pair<param::ParameterId, double>> ParamBefore;
    std::vector<std::pair<param::ParameterId, double>> ParamAfter;
    bool WasSuppressed{false};
};

class FeatureHistory
{
public:
    void Record(FeatureTransaction tx);
    void ApplyAndRecord(Part& part, FeatureTransaction tx);

    bool Undo(Part& part);
    bool Redo(Part& part);

    void Clear();
    [[nodiscard]] bool CanUndo() const noexcept
    {
        return m_index > 0;
    }
    [[nodiscard]] bool CanRedo() const noexcept
    {
        return m_index < static_cast<int>(m_entries.size());
    }

private:
    bool ApplyForward(Part& part, FeatureTransaction& tx);
    bool ApplyReverse(Part& part, FeatureTransaction& tx);

    std::vector<FeatureTransaction> m_entries;
    int m_index{0};
};

}  // namespace brep::feat
