#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
    vec4 albedoColor;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D albedoMap;

void main() {
    vec3 n = normalize(vNormal);
    float ndotl = max(dot(n, normalize(-ubo.lightDir.xyz)), 0.0);
    vec3 albedo = texture(albedoMap, vUV).rgb;
    // Mix in a little fallback tint so unlit cracks still look wooden.
    albedo = mix(ubo.albedoColor.xyz, albedo, 0.92);
    vec3 color = albedo * (0.28 + 0.72 * ndotl);
    outColor = vec4(color, 1.0);
}
