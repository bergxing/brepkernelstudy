#pragma once

#include <string>

namespace brep {

/// Simple PBR-lite material description (albedo map for now).
struct Material {
  std::string name{"default"};
  /// Path to albedo / base-color image (PNG/JPG). Empty = solid color fallback.
  std::string albedo_path;
  /// Multiplier applied to mesh UVs when sampling the albedo map.
  float uv_scale{1.0f};
  /// Fallback solid color (linear RGB) when no texture is available.
  float albedo_color[3]{0.45f, 0.62f, 0.85f};
};

inline Material make_wood_material(std::string albedo_path) {
  Material m;
  m.name = "wood";
  m.albedo_path = std::move(albedo_path);
  m.uv_scale = 1.0f;
  m.albedo_color[0] = 0.55f;
  m.albedo_color[1] = 0.35f;
  m.albedo_color[2] = 0.18f;
  return m;
}

}  // namespace brep
