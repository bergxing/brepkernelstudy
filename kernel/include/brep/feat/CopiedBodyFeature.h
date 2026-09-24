#pragma once

#include "brep/feat/Feature.h"
#include "brep/Plane.h"

#include <memory>
#include <string>

namespace brep::feat
{

/// Derived copy of another feature's current body + rigid translation.
/// Rebuild re-copies from the live source; deleting the source fails Rebuild.
class CopiedBodyFeature final : public IFeature
{
 public:
    CopiedBodyFeature(FeatureId id, std::string name, FeatureId source,
                      RigidTransform transform);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "CopiedBody";
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

    [[nodiscard]] FeatureId SourceFeatureId() const noexcept
    {
        return m_sourceFeature;
    }
    [[nodiscard]] const RigidTransform& Transform() const noexcept
    {
        return m_transform;
    }

    static std::unique_ptr<CopiedBodyFeature> Create(FeatureId source,
                                                     RigidTransform transform,
                                                     std::string name = "Copy");

 private:
    FeatureId m_id{};
    std::string m_name;
    FeatureId m_sourceFeature{};
    RigidTransform m_transform{};
    Guid m_bodyGuid{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
