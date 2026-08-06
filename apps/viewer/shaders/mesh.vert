#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
    vec4 albedoColor; // xyz = fallback RGB, w = uv_scale
} ubo;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;

void main() {
    vNormal = mat3(ubo.model) * inNormal;
    vUV = inUV * ubo.albedoColor.w;
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
