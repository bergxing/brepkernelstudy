#include "commands/picking.hpp"

#include <Eigen/Dense>

#include <cmath>

namespace brep::viewer::commands {
namespace {

Eigen::Matrix4f to_eigen(const float m[16]) {
  Eigen::Matrix4f M;
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      M(r, c) = m[c * 4 + r];
    }
  }
  return M;
}

}  // namespace

bool screen_to_ray(const Camera& cam, int viewport_w, int viewport_h, float sx,
                   float sy, Point3d& out_origin, Vector3d& out_dir) {
  if (viewport_w <= 0 || viewport_h <= 0) return false;

  const float aspect = float(viewport_w) / float(viewport_h);
  float view[16];
  float proj[16];
  cam.view_matrix(view);
  if (cam.ortho) {
    const float half_h = cam.ortho_half_h;
    const float half_w = half_h * aspect;
    Camera::ortho_matrix(half_w, half_h, 0.05f, 500.0f, proj);
  } else {
    Camera::perspective(cam.fov_deg, aspect, 0.05f, 500.0f, proj);
  }

  const Eigen::Matrix4f V = to_eigen(view);
  const Eigen::Matrix4f P = to_eigen(proj);
  const Eigen::Matrix4f inv = (P * V).inverse();

  // Qt: y down. NDC y: up. Projection already flips Y for Vulkan.
  const float ndc_x = (2.0f * sx / float(viewport_w)) - 1.0f;
  const float ndc_y = (2.0f * sy / float(viewport_h)) - 1.0f;

  auto unproject = [&](float z_ndc) -> Eigen::Vector3f {
    Eigen::Vector4f clip(ndc_x, ndc_y, z_ndc, 1.0f);
    Eigen::Vector4f world = inv * clip;
    if (std::abs(world.w()) < 1e-8f) return Eigen::Vector3f::Zero();
    world /= world.w();
    return world.head<3>();
  };

  const Eigen::Vector3f p_near = unproject(0.0f);
  const Eigen::Vector3f p_far = unproject(1.0f);
  Eigen::Vector3f dir = p_far - p_near;
  if (dir.norm() < 1e-8f) return false;
  dir.normalize();

  out_origin = Point3d{p_near.x(), p_near.y(), p_near.z()};
  out_dir = Vector3d{dir.x(), dir.y(), dir.z()};
  return true;
}

bool intersect_plane_y(const Point3d& origin, const Vector3d& dir, double plane_y,
                       Point3d& out_hit) {
  if (std::abs(dir.y()) < 1e-9) return false;
  const double t = (plane_y - origin.y()) / dir.y();
  if (t < 0.0) return false;
  out_hit = origin + dir * t;
  return true;
}

}  // namespace brep::viewer::commands
