#pragma once

#include "brep/feat/Feature.h"
#include "brep/feat/PrimitiveSpecs.h"
#include "brep/Math.h"

#include <memory>
#include <vector>

namespace brep::feat
{

class NurbsCurveFeature final : public IFeature
{
public:
    NurbsCurveFeature(FeatureId id, std::string name, NurbsCurveSpec spec);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "NurbsCurve";
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

    [[nodiscard]] const std::vector<Point3d>& Cvs() const noexcept
    {
        return m_cvs;
    }
    [[nodiscard]] const std::vector<double>& Weights() const noexcept
    {
        return m_weights;
    }
    [[nodiscard]] const std::vector<double>& Knots() const noexcept
    {
        return m_knots;
    }
    [[nodiscard]] int Degree() const noexcept
    {
        return m_degree;
    }
    [[nodiscard]] double Tolerance() const noexcept
    {
        return m_tolerance;
    }

    void SetFromSpec(const NurbsCurveSpec& spec);
    void SetTolerance(double tol) noexcept
    {
        m_tolerance = tol;
    }

    [[nodiscard]] NurbsCurveSpec ToSpec() const;
    [[nodiscard]] std::optional<PrimitiveSpec> ToPrimitiveSpec(
        const param::ParameterStore& params) const override
    {
        (void)params;
        return ToSpec();
    }

    static std::unique_ptr<NurbsCurveFeature> Create(
        param::ParameterStore& store, const NurbsCurveSpec& spec);

private:
    FeatureId m_id{};
    std::string m_name;
    std::vector<Point3d> m_cvs;
    std::vector<double> m_weights;
    std::vector<double> m_knots;
    int m_degree{3};
    double m_tolerance{1e-7};
    Guid m_bodyGuid{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
