#pragma once

#include "brep/math.hpp"

#include <algorithm>
#include <cmath>

namespace brep::viewer {

struct Camera {
  float yaw_deg{-35.0f};
  float pitch_deg{-25.0f};
  float distance{6.0f};
  float fov_deg{45.0f};
  float ortho_half_h{2.1f};  // world units (vertical half-extent)
  Point3d target{1.0, 0.5, 1.5};
  bool ortho{false};

  /// When true, eye/up come from framed_forward / framed_up (CAD snaps).
  bool framed{false};
  Vector3d framed_forward{0, 0, -1};  // eye → target
  Vector3d framed_up{0, 1, 0};

  void orbit(float dx, float dy) {
    exit_framed_to_orbit();
    ortho = false;
    yaw_deg += dx * 0.35f;
    pitch_deg += dy * 0.35f;
    if (pitch_deg > 89.0f) pitch_deg = 89.0f;
    if (pitch_deg < -89.0f) pitch_deg = -89.0f;
  }

  void pan(float dx, float dy) {
    const Point3d e = eye();
    Vector3d forward = (target - e).normalized();
    Vector3d up = framed ? framed_up.normalized() : Vector3d{0, 1, 0};
    Vector3d right = forward.cross(up);
    if (right.norm() < 1e-6) {
      up = Vector3d{0, 0, 1};
      right = forward.cross(up);
    }
    right = right.normalized();
    up = right.cross(forward).normalized();
    const float scale =
        (ortho ? ortho_half_h : distance * std::tan(fov_deg * 0.008726646f)) *
        0.005f;
    target = target + right * double(-dx * scale) + up * double(dy * scale);
  }

  /// Zoom in (delta>0) / out (delta<0). Ortho scales frustum; perspective
  /// scales FOV first (avoids dollying into the solid), then gently dollies.
  void zoom(float delta) {
    const bool zoom_in = delta > 0.0f;
    if (ortho) {
      ortho_half_h *= zoom_in ? 0.9f : 1.1f;
      if (ortho_half_h < 0.05f) ortho_half_h = 0.05f;
      if (ortho_half_h > 100.0f) ortho_half_h = 100.0f;
    } else {
      fov_deg *= zoom_in ? 0.9f : 1.1f;
      if (fov_deg < 10.0f) fov_deg = 10.0f;
      if (fov_deg > 75.0f) fov_deg = 75.0f;
      distance *= zoom_in ? 0.92f : 1.08f;
      if (distance < 2.5f) distance = 2.5f;
      if (distance > 200.0f) distance = 200.0f;
    }
  }

  void set_yaw_pitch(float yaw, float pitch) {
    framed = false;
    yaw_deg = yaw;
    pitch_deg = pitch;
    if (pitch_deg > 89.0f) pitch_deg = 89.0f;
    if (pitch_deg < -89.0f) pitch_deg = -89.0f;
  }

  /// Snap to a CAD orthographic view (Z-up naming: Top = look from +Z).
  void set_standard_view(char face) {
    switch (face) {
      case 'r':
        frame_view({-1, 0, 0}, {0, 1, 0}, true);
        yaw_deg = 0.0f;
        pitch_deg = 0.0f;
        break;
      case 'l':
        frame_view({1, 0, 0}, {0, 1, 0}, true);
        yaw_deg = 180.0f;
        pitch_deg = 0.0f;
        break;
      case 't':  // TOP +Z: X right, Y up, Z out of screen
        frame_view({0, 0, -1}, {0, 1, 0}, true);
        yaw_deg = 90.0f;
        pitch_deg = 0.0f;
        break;
      case 'b':
        frame_view({0, 0, 1}, {0, 1, 0}, true);
        yaw_deg = -90.0f;
        pitch_deg = 0.0f;
        break;
      case 'f':
        frame_view({0, -1, 0}, {0, 0, -1}, true);
        yaw_deg = 90.0f;
        pitch_deg = 89.0f;
        break;
      case 'k':
        frame_view({0, 1, 0}, {0, 0, -1}, true);
        yaw_deg = -90.0f;
        pitch_deg = -89.0f;
        break;
      case 'h':
        framed = false;
        ortho = false;
        fov_deg = 45.0f;
        distance = 6.0f;
        set_yaw_pitch(-35.0f, -25.0f);
        break;
      default:
        break;
    }
  }

  [[nodiscard]] Point3d eye() const {
    if (framed) {
      const Vector3d f = framed_forward.normalized();
      return target - f * double(distance);
    }
    const float yaw = yaw_deg * 0.01745329252f;
    const float pitch = pitch_deg * 0.01745329252f;
    const float cp = std::cos(pitch);
    return target + Vector3d{
        distance * cp * std::cos(yaw),
        distance * std::sin(pitch),
        distance * cp * std::sin(yaw),
    };
  }

  void view_matrix(float out[16]) const {
    const Point3d e = eye();
    Vector3d f =
        framed ? framed_forward.normalized() : (target - e).normalized();
    Vector3d up = framed ? framed_up.normalized() : Vector3d{0, 1, 0};
    Vector3d s = f.cross(up);
    if (s.norm() < 1e-6) {
      up = std::fabs(f.y()) > 0.9 ? Vector3d{0, 0, -1} : Vector3d{0, 1, 0};
      s = f.cross(up);
    }
    s = s.normalized();
    Vector3d u = s.cross(f);

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

  void orientation_view_matrix(float out[16]) const {
    Vector3d f =
        framed ? framed_forward.normalized() : orbit_forward().normalized();
    Vector3d up = framed ? framed_up.normalized() : Vector3d{0, 1, 0};
    Vector3d s = f.cross(up);
    if (s.norm() < 1e-6) {
      up = std::fabs(f.y()) > 0.9 ? Vector3d{0, 0, -1} : Vector3d{0, 1, 0};
      s = f.cross(up);
    }
    s = s.normalized();
    Vector3d u = s.cross(f);
    const Point3d e = Point3d{0, 0, 0} - f * 3.0;

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

  static void perspective(float fovy_deg, float aspect, float znear, float zfar,
                          float out[16]) {
    const float f = 1.0f / std::tan(fovy_deg * 0.01745329252f * 0.5f);
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = f / aspect;
    out[5] = -f;  // Vulkan Y flip
    out[10] = zfar / (znear - zfar);
    out[11] = -1.0f;
    out[14] = (zfar * znear) / (znear - zfar);
  }

  static void ortho_matrix(float half_w, float half_h, float znear, float zfar,
                           float out[16]) {
    for (int i = 0; i < 16; ++i) out[i] = 0.0f;
    out[0] = 1.0f / half_w;
    out[5] = -1.0f / half_h;  // Vulkan Y flip
    out[10] = 1.0f / (znear - zfar);
    out[14] = znear / (znear - zfar);
    out[15] = 1.0f;
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

 private:
  void frame_view(Vector3d forward, Vector3d up, bool use_ortho) {
    framed = true;
    ortho = use_ortho;
    framed_forward = forward.normalized();
    framed_up = up.normalized();
    if (use_ortho) {
      // Keep a sensible on-screen size when entering ortho.
      ortho_half_h = std::max(0.5f, distance * 0.35f);
    }
  }

  void exit_framed_to_orbit() {
    if (!framed) return;
    const Point3d e = eye();
    const Vector3d d = (e - target).normalized();
    pitch_deg = static_cast<float>(std::asin(std::clamp(d.y(), -1.0, 1.0)) *
                                   57.29577951308232);
    yaw_deg = static_cast<float>(std::atan2(d.z(), d.x()) * 57.29577951308232);
    framed = false;
  }

  [[nodiscard]] Vector3d orbit_forward() const {
    const float yaw = yaw_deg * 0.01745329252f;
    const float pitch = pitch_deg * 0.01745329252f;
    const float cp = std::cos(pitch);
    return Vector3d{
        -cp * std::cos(yaw),
        -std::sin(pitch),
        -cp * std::sin(yaw),
    };
  }
};

}  // namespace brep::viewer
