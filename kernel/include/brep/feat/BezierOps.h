#pragma once

#include "brep/feat/PrimitiveSpecs.h"
#include "brep/Math.h"

#include <algorithm>
#include <vector>

namespace brep
{

/// Raise degree by 1 (shape-preserving). Single-segment only.
[[nodiscard]] inline BezierSpec ElevateBezierDegree(const BezierSpec& spec)
{
    BezierSpec out = spec;
    if (spec.SegmentCount != 1 || spec.Cvs.size() < 2)
    {
        return out;
    }
    const int n = static_cast<int>(spec.Cvs.size()) - 1;
    std::vector<Point3d> q;
    q.reserve(static_cast<std::size_t>(n) + 2U);
    q.push_back(spec.Cvs.front());
    for (int i = 1; i <= n; ++i)
    {
        const double a = static_cast<double>(i) / static_cast<double>(n + 1);
        const double b = 1.0 - a;
        const Point3d& pPrev = spec.Cvs[static_cast<std::size_t>(i - 1)];
        const Point3d& p = spec.Cvs[static_cast<std::size_t>(i)];
        q.push_back(Point3d{a * pPrev.x() + b * p.x(), a * pPrev.y() + b * p.y(),
                            a * pPrev.z() + b * p.z()});
    }
    q.push_back(spec.Cvs.back());
    out.Cvs = std::move(q);
    out.Degree = n + 1;
    out.SegmentCount = 1;
    out.Corner.clear();
    if (!spec.Weights.empty())
    {
        std::vector<double> wq;
        wq.reserve(static_cast<std::size_t>(n) + 2U);
        wq.push_back(BezierWeightAt(spec, 0));
        for (int i = 1; i <= n; ++i)
        {
            const double a =
                static_cast<double>(i) / static_cast<double>(n + 1);
            const double b = 1.0 - a;
            const double wPrev = BezierWeightAt(spec, static_cast<std::size_t>(i - 1));
            const double w = BezierWeightAt(spec, static_cast<std::size_t>(i));
            const Point3d& pPrev = spec.Cvs[static_cast<std::size_t>(i - 1)];
            const Point3d& p = spec.Cvs[static_cast<std::size_t>(i)];
            const double hw = a * wPrev + b * w;
            if (hw > 1e-15)
            {
                out.Cvs[static_cast<std::size_t>(i)] = Point3d{
                    (a * wPrev * pPrev.x() + b * w * p.x()) / hw,
                    (a * wPrev * pPrev.y() + b * w * p.y()) / hw,
                    (a * wPrev * pPrev.z() + b * w * p.z()) / hw};
            }
            wq.push_back(hw);
        }
        wq.push_back(BezierWeightAt(spec, spec.Cvs.size() - 1));
        out.Weights = std::move(wq);
    }
    return out;
}

/// G1 at internal anchor `anchorIndex` (1 .. SegmentCount-1): keep out-handle
/// length, force in-handle opposite collinear (or vice versa if preferOut=false).
inline void EnforceBezierG1(BezierSpec& spec, int anchorIndex, bool moveIn)
{
    if (spec.Degree != 3 || spec.SegmentCount < 2)
    {
        return;
    }
    if (anchorIndex <= 0 || anchorIndex >= spec.SegmentCount)
    {
        return;
    }
    const int joint = anchorIndex;  // Cvs index 3*joint
    const std::size_t a = static_cast<std::size_t>(3 * joint);
    const std::size_t inTip = a - 1;
    const std::size_t outTip = a + 1;
    if (outTip >= spec.Cvs.size())
    {
        return;
    }
    const Point3d& anchor = spec.Cvs[a];
    if (moveIn)
    {
        Vector3d out = spec.Cvs[outTip] - anchor;
        const double outLen = out.norm();
        if (outLen < 1e-15)
        {
            return;
        }
        out = out.normalized();
        const double inLen = (anchor - spec.Cvs[inTip]).norm();
        spec.Cvs[inTip] = anchor - out * std::max(inLen, 1e-9);
    }
    else
    {
        Vector3d in = anchor - spec.Cvs[inTip];
        const double inLen = in.norm();
        if (inLen < 1e-15)
        {
            return;
        }
        in = in.normalized();
        const double outLen = (spec.Cvs[outTip] - anchor).norm();
        spec.Cvs[outTip] = anchor + in * std::max(outLen, 1e-9);
    }
}

[[nodiscard]] inline bool BezierJointIsCorner(const BezierSpec& spec,
                                              int anchorIndex) noexcept
{
    if (anchorIndex <= 0 || anchorIndex >= spec.SegmentCount)
    {
        return false;
    }
    const std::size_t i = static_cast<std::size_t>(anchorIndex - 1);
    return i < spec.Corner.size() && spec.Corner[i] != 0;
}

}  // namespace brep
