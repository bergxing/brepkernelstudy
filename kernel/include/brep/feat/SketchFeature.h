#pragma once

#include "brep/feat/Feature.h"
#include "brep/Math.h"
#include "brep/Plane.h"
#include "brep/sketch/Sketch.h"

#include <memory>
#include <string>

namespace brep::feat
{

class SketchFeature final : public IFeature
{
public:
    SketchFeature(FeatureId id, std::string name, sketch::Sketch sketch);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "Sketch";
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
        return {};
    }
    void SetBodyGuid(Guid) override
    {
    }
    [[nodiscard]] std::string DisplayName() const override
    {
        return m_name;
    }

    [[nodiscard]] sketch::Sketch& Sketch() noexcept
    {
        return m_sketch;
    }
    [[nodiscard]] const sketch::Sketch& Sketch() const noexcept
    {
        return m_sketch;
    }

    static std::unique_ptr<SketchFeature> CreateRectangle(
        param::ParameterStore& store, std::string name, Point2d min,
        Point2d max, brep::Plane frame = brep::Plane::XzYUp());

private:
    FeatureId m_id{};
    std::string m_name;
    sketch::Sketch m_sketch{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
