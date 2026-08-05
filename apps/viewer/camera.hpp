#pragma once

#include "brep/math.hpp"

#include <cmath>

namespace brep::viewer {

struct Camera {
  float yaw_deg{-35.0f};
  float pitch_deg{-25.0f};
  float distance{6.0f};
  Point3d target{1.0, 0.5, 1.5};

  void orbit(float dx, float dy) {
    yaw_deg += dx * 0.35f;
    pitch_deg += dy * 0.35f;
    if (pitch_deg > 89.0f) pitch_deg = 89.0f;
    if (pitch_deg < -89.0f) pitch_deg = -89.0f;
  }

  void zoom(float delta) {
    distance *= (delta > 0.0f) ? 0.9f : 1.1f;
    if (distance < 0.3f) distance = 0.3f;
    if (distance > 200.0f) distance = 200.0f;
  }

  [[nodiscard]] Point3d eye() const {
    const float yaw = yaw_deg * 0.01745329252f;
    const float pitch = pitch_deg * 0.01745329252f;
    const float cp = std::cos(pitch);
    return target + Vector3d{
        distance * cp * std::cos(yaw),
        distance * std::sin(pitch),
        distance * cp * std::sin(yaw),
    };
  }

  // Column-major 4x4 matrices for Vulkan (same as OpenGL-style GLM layout).
  [[nodiscard]] void view_matrix(float out[16]) const {
    const Point3d e = eye();
    Vector3d f = (target - e).normalized();
    Vector3d up{0, 1, 0};
    Vector3d s = f.cross(up).normalized();
    if (s.norm() < 1e-6) {
      up = Vector3d{0, 0, 1};
      s = f.cross(up).normalized();
    }
    Vector3d u = s.cross(f);

    // view = lookAt
    out[0] = static_cast<float>(s.x());
    out[1] = static_cast<float>(u.x());
    out[2] = static_cast<float>(-f.x());
    out[3] = 0.0f;
    out[4] = static_cast<float>(s.y());
    out[5] = static_cast<float>(u.y());
    out[6] = static_cast<float>(-f.y());
    out[7] = 0.0f;
    out[8] = static_cast<float>(s.z());
    out[9] = static_cast<float>(u.z());
    out[10] = static_cast<float>(-f.z());
    out[11] = 0.0f;
    out[12] = static_cast<float>(-s.dot(Vector3d{e.x(), e.y(), e.z()}));
    out[13] = static_cast<float>(-u.dot(Vector3d{e.x(), e.y(), e.z()}));
    out[14] = static_cast<float>(f.dot(Vector3d{e.x(), e.y(), e.z()}));
    out[15] = 1.0f;
  }

  [[nodiscard]] static void perspective(float fovy_deg, float aspect, float znear,
                                        float zfar, float out[16]) {
    const float f = 1.0f / std::tan(fovy_deg * 0.01745329252f * 0.5f);
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = f / aspect;
    out[5] = -f;  // Vulkan Y flip
    out[10] = zfar / (znear - zfar);
    out[11] = -1.0f;
    out[14] = (zfar * znear) / (znear - zfar);
  }

  static void multiply(const float a[16], const float b[16], float out[16]) {
    float r[16];
    for (int c = 0; c < 4; ++c) {
      for (int row = 0; row < 4; ++row) {
        r[c * 4 + row] = a[0 * 4 + row] * b[c * 4 + 0] +
                         a[1 * 4 + row] * b[c * 4 + 1] +
                         a[2 * 4 + row] * b[c * 4 + 2] +
                         a[3 * 4 + row] * b[c * 4 + 3];
      }
    }
    for (int i = 0; i < 16; ++i) out[i] = r[i];
  }

  static void identity(float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = out[5] = out[10] = out[15] = 1.0f;
  }
};

}  // namespace brep::viewer
