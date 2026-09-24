#pragma once

#include "brep/Math.h"
#include "brep/Plane.h"

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <variant>

namespace brep
{

struct BoxSpec
{
    Point3d Min{0, 0, 0};
    Point3d Max{1, 1, 1};
    double Tolerance{1e-7};
    std::string Name{"box"};
};

struct SphereSpec
{
    Point3d Center{0, 0, 0};
    double Radius{1.0};
    int Slices{16};
    int Stacks{12};
    double Tolerance{1e-7};
    std::string Name{"sphere"};
};

/// Rational Bézier (single segment degree 1–3) or cubic polycurve.
/// - SegmentCount==1: Cvs.size()==Degree+1 (line / parabola / cubic)
/// - SegmentCount>1: Degree must be 3, Cvs.size()==3*SegmentCount+1
/// - Weights empty → all 1; else Weights.size()==Cvs.size() and each > 0
/// Corner[i]==1 → internal anchor i+1 is G0 corner (else G1).
struct BezierSpec
{
    std::vector<Point3d> Cvs{Point3d{}, Point3d{}, Point3d{}, Point3d{}};
    std::vector<double> Weights{};
    int Degree{3};
    int SegmentCount{1};
    std::vector<std::uint8_t> Corner{};
    double Tolerance{1e-7};
    std::string Name{"bezier"};

    [[nodiscard]] Point3d& CvAt(std::size_t i) noexcept
    {
        if (Cvs.size() <= i)
        {
            Cvs.resize(i + 1);
        }
        return Cvs[i];
    }
    [[nodiscard]] const Point3d& CvAt(std::size_t i) const noexcept
    {
        static const Point3d kOrigin{};
        return i < Cvs.size() ? Cvs[i] : kOrigin;
    }
    [[nodiscard]] Point3d& P0() noexcept
    {
        return CvAt(0);
    }
    [[nodiscard]] Point3d& P1() noexcept
    {
        return CvAt(1);
    }
    [[nodiscard]] Point3d& P2() noexcept
    {
        return CvAt(2);
    }
    [[nodiscard]] Point3d& P3() noexcept
    {
        return CvAt(3);
    }
    [[nodiscard]] const Point3d& P0() const noexcept
    {
        return CvAt(0);
    }
    [[nodiscard]] const Point3d& P1() const noexcept
    {
        return CvAt(1);
    }
    [[nodiscard]] const Point3d& P2() const noexcept
    {
        return CvAt(2);
    }
    [[nodiscard]] const Point3d& P3() const noexcept
    {
        return CvAt(3);
    }
};

[[nodiscard]] inline double BezierWeightAt(const BezierSpec& spec,
                                           std::size_t i) noexcept
{
    if (i < spec.Weights.size() && spec.Weights[i] > 0.0)
    {
        return spec.Weights[i];
    }
    return 1.0;
}

[[nodiscard]] inline bool BezierWeightsAreUnit(const BezierSpec& spec) noexcept
{
    if (spec.Weights.empty())
    {
        return true;
    }
    if (spec.Weights.size() != spec.Cvs.size())
    {
        return false;
    }
    for (double w : spec.Weights)
    {
        if (std::abs(w - 1.0) > 1e-12)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool BezierSpecValid(const BezierSpec& spec) noexcept
{
    if (spec.Cvs.size() < 2 || spec.Degree < 1 || spec.SegmentCount < 1)
    {
        return false;
    }
    if (!spec.Weights.empty())
    {
        if (spec.Weights.size() != spec.Cvs.size())
        {
            return false;
        }
        for (double w : spec.Weights)
        {
            if (!(w > 0.0))
            {
                return false;
            }
        }
    }
    if (spec.SegmentCount == 1)
    {
        return static_cast<int>(spec.Cvs.size()) == spec.Degree + 1;
    }
    return spec.Degree == 3 &&
           static_cast<int>(spec.Cvs.size()) == 3 * spec.SegmentCount + 1;
}

struct NurbsCurveSpec
{
    std::vector<Point3d> Cvs{};
    std::vector<double> Weights{};
    std::vector<double> Knots{};
    int Degree{3};
    double Tolerance{1e-7};
    std::string Name{"nurbs"};
};

[[nodiscard]] inline double NurbsWeightAt(const NurbsCurveSpec& spec,
                                          std::size_t i) noexcept
{
    if (i < spec.Weights.size() && spec.Weights[i] > 0.0)
    {
        return spec.Weights[i];
    }
    return 1.0;
}

[[nodiscard]] inline std::vector<double> ClampedUniformKnots(int cvCount,
                                                             int degree)
{
    const int n = cvCount - 1;
    const int knotCount = cvCount + degree + 1;
    std::vector<double> u(static_cast<std::size_t>(knotCount), 0.0);
    const int interior = n - degree;
    for (int j = 1; j <= interior; ++j)
    {
        u[static_cast<std::size_t>(degree + j)] =
            static_cast<double>(j) / static_cast<double>(interior + 1);
    }
    for (int i = n + 1; i < knotCount; ++i)
    {
        u[static_cast<std::size_t>(i)] = 1.0;
    }
    return u;
}

[[nodiscard]] inline bool NurbsCurveSpecValid(
    const NurbsCurveSpec& spec) noexcept
{
    if (spec.Degree != 3 || spec.Cvs.size() < 4 || spec.Cvs.size() > 256)
    {
        return false;
    }
    if (!spec.Weights.empty())
    {
        if (spec.Weights.size() != spec.Cvs.size())
        {
            return false;
        }
        for (double w : spec.Weights)
        {
            if (!(w > 0.0))
            {
                return false;
            }
        }
    }
    if (spec.Knots.empty())
    {
        return true;
    }
    if (spec.Knots.size() != spec.Cvs.size() + 4)
    {
        return false;
    }
    for (std::size_t i = 1; i < spec.Knots.size(); ++i)
    {
        if (spec.Knots[i] < spec.Knots[i - 1])
        {
            return false;
        }
    }
    for (int i = 0; i < 4; ++i)
    {
        if (spec.Knots[static_cast<std::size_t>(i)] != 0.0)
        {
            return false;
        }
        if (spec.Knots[spec.Knots.size() - 1 - static_cast<std::size_t>(i)] !=
            1.0)
        {
            return false;
        }
    }
    for (std::size_t i = 4; i + 4 < spec.Knots.size(); ++i)
    {
        int run = 1;
        while (i + static_cast<std::size_t>(run) + 4 < spec.Knots.size() &&
               spec.Knots[i + static_cast<std::size_t>(run)] == spec.Knots[i])
        {
            ++run;
        }
        if (run > spec.Degree)
        {
            return false;
        }
    }
    return true;
}

/// Closed set of parametric primitives that can be reconstructed from a spec.
using PrimitiveSpec =
    std::variant<BoxSpec, SphereSpec, BezierSpec, NurbsCurveSpec>;

[[nodiscard]] inline std::optional<PrimitiveSpec> ApplyTransform(
    const PrimitiveSpec& spec, const RigidTransform& transform)
{
    if (!transform.IsTranslation())
    {
        return std::nullopt;
    }
    return std::visit(
        [&transform](auto s) -> PrimitiveSpec
        {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
            {
                s.Min = transform.TransformPoint(s.Min);
                s.Max = transform.TransformPoint(s.Max);
                return s;
            }
            else if constexpr (std::is_same_v<T, SphereSpec>)
            {
                s.Center = transform.TransformPoint(s.Center);
                return s;
            }
            else if constexpr (std::is_same_v<T, BezierSpec>)
            {
                for (Point3d& p : s.Cvs)
                {
                    p = transform.TransformPoint(p);
                }
                return s;
            }
            else
            {
                static_assert(std::is_same_v<T, NurbsCurveSpec>,
                              "ApplyTransform: add PrimitiveSpec arm");
                for (Point3d& p : s.Cvs)
                {
                    p = transform.TransformPoint(p);
                }
                return s;
            }
        },
        spec);
}

}  // namespace brep
