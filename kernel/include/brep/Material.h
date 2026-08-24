#pragma once

#include <string>

namespace brep
{

/// Simple PBR-lite material description (albedo map for now).
struct Material
{
    std::string Name{"default"};
    /// Path to albedo / base-color image (PNG/JPG). Empty = solid color fallback.
    std::string AlbedoPath;
    /// Multiplier applied to mesh UVs when sampling the albedo map.
    float UvScale{1.0f};
    /// Fallback solid color (linear RGB) when no texture is available.
    float AlbedoColor[3]{0.45f, 0.62f, 0.85f};
};

inline Material MakeWoodMaterial(std::string albedoPath)
{
    Material m;
    m.Name = "wood";
    m.AlbedoPath = std::move(albedoPath);
    m.UvScale = 1.0f;
    m.AlbedoColor[0] = 0.55f;
    m.AlbedoColor[1] = 0.35f;
    m.AlbedoColor[2] = 0.18f;
    return m;
}

}  // namespace brep
