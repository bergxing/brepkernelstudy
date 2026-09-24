#pragma once

#include "Camera.h"

#include "api/Core.h"
#include "api/Mesh.h"

namespace brep::viewer::commands
{

/// Screen pixel → world ray (Qt y down; matches Camera Vulkan projection).
bool ScreenToRay(const Camera& cam, int viewportW, int viewportH, float sx,
                 float sy, Point3d& outOrigin, Vector3d& outDir);

/// World point → screen pixel (same convention as ScreenToRay).
/// Returns false when behind the camera / not projectable.
bool WorldToScreen(const Camera& cam, int viewportW, int viewportH,
                   const Point3d& world, float& outSx, float& outSy);

/// Intersect ray with plane y = planeY. Returns false if parallel / behind.
bool IntersectPlaneY(const Point3d& origin, const Vector3d& dir, double planeY,
                     Point3d& outHit);

/// Intersect ray with a general plane. Returns false if parallel / behind.
bool IntersectPlane(const Point3d& origin, const Vector3d& dir,
                    const Point3d& planePoint, const Vector3d& planeNormal,
                    Point3d& outHit);

/// Closest ray/triangle hit along +dir. `originOffset` is added to each vertex
/// (entity Transform.position). Returns false if no hit with t >= 0.
bool IntersectMesh(const Point3d& origin, const Vector3d& dir,
                   const TriangleMesh& mesh, const Point3d& originOffset,
                   double& outT);

/// Screen-space edge pick for wires (empty triangle mesh). Hits when the
/// cursor is within `aperturePx` of a segment; `outT` is the ray parameter
/// to the closest 3D point (same convention as IntersectMesh).
bool IntersectEdges(const Camera& cam, int viewportW, int viewportH, float sx,
                    float sy, int aperturePx, const Point3d& rayOrigin,
                    const Vector3d& rayDir, const EdgeMesh& mesh,
                    const Point3d& originOffset, double& outT);

}  // namespace brep::viewer::commands
