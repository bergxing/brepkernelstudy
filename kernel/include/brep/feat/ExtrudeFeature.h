#pragma once

#include "brep/feat/Feature.h"
#include "brep/feat/SketchFeature.h"

#include <memory>

namespace brep::feat
{

class ExtrudeFeature final : public IFeature
{
public:
    ExtrudeFeature(FeatureId id, std::string name, FeatureId sketchFeature,
                   param::ParameterId distance);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "Extrude";
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

    [[nodiscard]] FeatureId SketchFeatureId() const noexcept
    {
        return m_sketchFeature;
    }
    [[nodiscard]] param::ParameterId DistanceId() const noexcept
    {
        return m_distance;
    }

    static std::unique_ptr<ExtrudeFeature> Create(
        param::ParameterStore& store, std::string name,
        FeatureId sketchFeature, double distance);

private:
    FeatureId m_id{};
    std::string m_name;
    FeatureId m_sketchFeature{};
    param::ParameterId m_distance{};
    Guid m_bodyGuid{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
