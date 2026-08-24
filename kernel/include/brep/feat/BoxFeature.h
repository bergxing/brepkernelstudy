#pragma once

#include "brep/Builder.h"
#include "brep/feat/Feature.h"
#include "brep/Math.h"

#include <memory>

namespace brep::feat
{

class BoxFeature final : public IFeature
{
public:
    BoxFeature(FeatureId id, std::string name, Point3d origin,
               param::ParameterId length, param::ParameterId width,
               param::ParameterId height);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "Box";
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

    [[nodiscard]] Point3d Origin() const noexcept
    {
        return m_origin;
    }
    void SetOrigin(Point3d o) noexcept
    {
        m_origin = o;
    }

    [[nodiscard]] param::ParameterId LengthId() const noexcept
    {
        return m_length;
    }
    [[nodiscard]] param::ParameterId WidthId() const noexcept
    {
        return m_width;
    }
    [[nodiscard]] param::ParameterId HeightId() const noexcept
    {
        return m_height;
    }

    [[nodiscard]] BoxSpec ToSpec(const param::ParameterStore& params) const;

    static std::unique_ptr<BoxFeature> Create(param::ParameterStore& store,
                                              const BoxSpec& spec);

private:
    FeatureId m_id{};
    std::string m_name;
    Point3d m_origin{};
    param::ParameterId m_length{};
    param::ParameterId m_width{};
    param::ParameterId m_height{};
    Guid m_bodyGuid{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
