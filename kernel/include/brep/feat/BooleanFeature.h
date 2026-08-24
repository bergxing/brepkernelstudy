#pragma once

#include "brep/bool/Types.h"
#include "brep/feat/Feature.h"

#include <memory>
#include <string>

namespace brep::feat
{

class BooleanFeature final : public IFeature
{
public:
    BooleanFeature(FeatureId id, std::string name, boolean::BooleanOp op,
                   FeatureId target, FeatureId tool);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "Boolean";
    }
    [[nodiscard]] FeatureStatus Status() const override
    {
        return m_status;
    }
    void SetStatus(FeatureStatus s) override
    {
        m_status = s;
    }
    void SetSuppressed(bool suppressed) override
    {
        m_suppressed = suppressed;
    }
    [[nodiscard]] bool Suppressed() const override
    {
        return m_suppressed;
    }

    void CollectParameters(param::ParameterStore& store) override;
    bool Rebuild(Part& part, param::ParameterStore& params) override;

    [[nodiscard]] Guid BodyGuid() const override
    {
        return m_bodyGuid;
    }
    void SetBodyGuid(Guid g) override
    {
        m_bodyGuid = g;
    }
    [[nodiscard]] std::string DisplayName() const override
    {
        return m_name;
    }

    [[nodiscard]] boolean::BooleanOp Op() const noexcept
    {
        return m_op;
    }
    [[nodiscard]] FeatureId TargetFeatureId() const noexcept
    {
        return m_target;
    }
    [[nodiscard]] FeatureId ToolFeatureId() const noexcept
    {
        return m_tool;
    }

    static std::unique_ptr<BooleanFeature> Create(boolean::BooleanOp op,
                                                  FeatureId target,
                                                  FeatureId tool,
                                                  std::string name = "Boolean");

private:
    FeatureId m_id{};
    std::string m_name;
    boolean::BooleanOp m_op{boolean::BooleanOp::Union};
    FeatureId m_target{};
    FeatureId m_tool{};
    Guid m_bodyGuid{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
