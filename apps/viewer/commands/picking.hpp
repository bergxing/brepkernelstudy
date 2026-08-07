#pragma once

#include "camera.hpp"

#include "brep/math.hpp"
#include "brep/mesh.hpp"

namespace brep::viewer::commands {

/// Screen pixel → world ray (Qt y down; matches Camera Vulkan projection).
bool screen_to_ray(const Camera& cam, int viewport_w, int viewport_h, float sx,
                   float sy, Point3d& out_origin, Vector3d& out_dir);

/// Intersect ray with plane y = plane_y. Returns false if parallel / behind.
bool intersect_plane_y(const Point3d& origin, const Vector3d& dir, double plane_y,
                       Point3d& out_hit);

/// Intersect ray with a general plane. Returns false if parallel / behind.
bool intersect_plane(const Point3d& origin, const Vector3d& dir,
                     const Point3d& plane_point, const Vector3d& plane_normal,
                     Point3d& out_hit);

/// Closest ray/triangle hit along +dir. `origin_offset` is added to each vertex
/// (entity Transform.position). Returns false if no hit with t >= 0.
bool intersect_mesh(const Point3d& origin, const Vector3d& dir,
                    const TriangleMesh& mesh, const Point3d& origin_offset,
                    double& out_t);

}  // namespace brep::viewer::commands
