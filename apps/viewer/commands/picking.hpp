#pragma once

#include "camera.hpp"

#include "brep/math.hpp"

namespace brep::viewer::commands {

/// Screen pixel → world ray (Qt y down; matches Camera Vulkan projection).
bool screen_to_ray(const Camera& cam, int viewport_w, int viewport_h, float sx,
                   float sy, Point3d& out_origin, Vector3d& out_dir);

/// Intersect ray with plane y = plane_y. Returns false if parallel / behind.
bool intersect_plane_y(const Point3d& origin, const Vector3d& dir, double plane_y,
                       Point3d& out_hit);

}  // namespace brep::viewer::commands
