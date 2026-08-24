#pragma once

#include "brep/Builder.h"
#include "brep/feat/Feature.h"
#include "brep/Math.h"

#include <memory>

namespace brep::feat
{

class SphereFeature final : public IFeature
{
public:
    SphereFeature(FeatureId id, std::string name, Point3d center,
                  param::ParameterId radius);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "Sphere";
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

    [[nodiscard]] Point3d Center() const noexcept
    {
        return m_center;
    }
    void SetCenter(Point3d c) noexcept
    {
        m_center = c;
    }

    [[nodiscard]] param::ParameterId RadiusId() const noexcept
    {
        return m_radius;
    }

    [[nodiscard]] SphereSpec ToSpec(const param::ParameterStore& params) const;

    static std::unique_ptr<SphereFeature> Create(param::ParameterStore& store,
                                                 const SphereSpec& spec);

private:
    FeatureId m_id{};
    std::string m_name;
    Point3d m_center{};
    param::ParameterId m_radius{};
    Guid m_bodyGuid{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
