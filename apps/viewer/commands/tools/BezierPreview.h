#pragma once

#include "commands/Picking.h"
#include "commands/tools/PreviewEdges.h"

#include "api/Core.h"
#include "api/Mesh.h"

#include <cmath>
#include <optional>

namespace brep::viewer::commands
{

inline void AppendPolylinePts(EdgeMesh& mesh, const std::vector<Point3d>& pts)
{
    for (std::size_t i = 1; i < pts.size(); ++i)
    {
        PushSegment(mesh, pts[i - 1], pts[i]);
    }
}

/// Curve sample + Pen overlay: anchors P0/P3, handle stems, all four tips.
inline void AppendBezierPenHandles(EdgeMesh& mesh, const Point3d& p0,
                                   const Point3d& p1, const Point3d& p2,
                                   const Point3d& p3)
{
    PushSegment(mesh, p0, p1);
    PushSegment(mesh, p3, p2);
    for (const Point3d& p : {p0, p1, p2, p3})
    {
        EdgeMesh tip = MakePointMarker(p);
        mesh.Positions.insert(mesh.Positions.end(), tip.Positions.begin(),
                              tip.Positions.end());
    }
}

inline void AppendBezierCurvePreview(EdgeMesh& mesh,
                                     const std::vector<Point3d>& cvs,
                                     const std::vector<double>& weights = {})
{
    if (cvs.size() < 2)
    {
        return;
    }
    BezierCurve curve(cvs, weights);
    AppendPolylinePts(mesh, SampleBezierPolyline(curve, 32));
}

inline void AppendBezierPenPreview(EdgeMesh& mesh, const Point3d& p0,
                                   const Point3d& p1, const Point3d& p2,
                                   const Point3d& p3)
{
    BezierCurve curve(p0, p1, p2, p3);
    AppendPolylinePts(mesh, SampleBezierPolyline(curve, 32));
    AppendBezierPenHandles(mesh, p0, p1, p2, p3);
}

[[nodiscard]] inline EdgeMesh MakeBezierPenPreview(const Point3d& p0,
                                                   const Point3d& p1,
                                                   const Point3d& p2,
                                                   const Point3d& p3)
{
    EdgeMesh mesh;
    AppendBezierPenPreview(mesh, p0, p1, p2, p3);
    return mesh;
}

/// Screen-space hit on any CV. Returns index into cvs.
[[nodiscard]] inline std::optional<int> HitTestBezierCvs(
    const Camera& cam, int viewportW, int viewportH, float sx, float sy,
    int aperturePx, const std::vector<Point3d>& cvs,
    const Point3d& originOffset = {})
{
    if (viewportW <= 0 || viewportH <= 0 || aperturePx < 0 || cvs.empty())
    {
        return std::nullopt;
    }
    const float aperture2 =
        static_cast<float>(aperturePx) * static_cast<float>(aperturePx);
    int best = -1;
    float bestDist2 = aperture2;
    for (std::size_t i = 0; i < cvs.size(); ++i)
    {
        const Point3d world{cvs[i].x() + originOffset.x(),
                            cvs[i].y() + originOffset.y(),
                            cvs[i].z() + originOffset.z()};
        float px = 0.0f;
        float py = 0.0f;
        if (!WorldToScreen(cam, viewportW, viewportH, world, px, py))
        {
            continue;
        }
        const float dx = sx - px;
        const float dy = sy - py;
        const float d2 = dx * dx + dy * dy;
        if (d2 <= bestDist2)
        {
            bestDist2 = d2;
            best = static_cast<int>(i);
        }
    }
    if (best < 0)
    {
        return std::nullopt;
    }
    return best;
}

[[nodiscard]] inline std::optional<int> HitTestBezierCv(
    const Camera& cam, int viewportW, int viewportH, float sx, float sy,
    int aperturePx, const Point3d cvs[4], const Point3d& originOffset = {})
{
    return HitTestBezierCvs(cam, viewportW, viewportH, sx, sy, aperturePx,
                            {cvs[0], cvs[1], cvs[2], cvs[3]}, originOffset);
}

}  // namespace brep::viewer::commands
