#pragma once

#include "brep/feat/Feature.h"
#include "brep/feat/PrimitiveSpecs.h"
#include "brep/Math.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace brep::feat
{

class BezierCurveFeature final : public IFeature
{
public:
    BezierCurveFeature(FeatureId id, std::string name, BezierSpec spec);

    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }
    [[nodiscard]] std::string_view TypeName() const override
    {
        return "Bezier";
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
    [[nodiscard]] Point3d P0() const noexcept
    {
        return m_cvs.empty() ? Point3d{} : m_cvs.front();
    }
    [[nodiscard]] Point3d P1() const noexcept
    {
        return m_cvs.size() > 1 ? m_cvs[1] : Point3d{};
    }
    [[nodiscard]] Point3d P2() const noexcept
    {
        return m_cvs.size() > 2 ? m_cvs[2] : Point3d{};
    }
    [[nodiscard]] Point3d P3() const noexcept
    {
        return m_cvs.empty() ? Point3d{} : m_cvs.back();
    }
    [[nodiscard]] int Degree() const noexcept
    {
        return m_degree;
    }
    [[nodiscard]] int SegmentCount() const noexcept
    {
        return m_segmentCount;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& Corner() const noexcept
    {
        return m_corner;
    }
    [[nodiscard]] const std::vector<double>& Weights() const noexcept
    {
        return m_weights;
    }
    [[nodiscard]] double Tolerance() const noexcept
    {
        return m_tolerance;
    }

    void SetFromSpec(const BezierSpec& spec);
    void SetControlPoints(Point3d p0, Point3d p1, Point3d p2,
                          Point3d p3) noexcept
    {
        m_cvs = {p0, p1, p2, p3};
        m_degree = 3;
        m_segmentCount = 1;
        m_corner.clear();
        m_weights.clear();
    }
    void SetTolerance(double tol) noexcept
    {
        m_tolerance = tol;
    }

    [[nodiscard]] BezierSpec ToSpec() const;
    [[nodiscard]] std::optional<PrimitiveSpec> ToPrimitiveSpec(
        const param::ParameterStore& params) const override
    {
        (void)params;
        return ToSpec();
    }

    static std::unique_ptr<BezierCurveFeature> Create(
        param::ParameterStore& store, const BezierSpec& spec);

private:
    FeatureId m_id{};
    std::string m_name;
    std::vector<Point3d> m_cvs;
    int m_degree{3};
    int m_segmentCount{1};
    std::vector<std::uint8_t> m_corner;
    std::vector<double> m_weights;
    double m_tolerance{1e-7};
    Guid m_bodyGuid{};
    FeatureStatus m_status{FeatureStatus::Dirty};
    bool m_suppressed{false};
};

}  // namespace brep::feat
