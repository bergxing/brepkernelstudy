#version 450
layout(location = 0) in vec3 vNormal;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
} ubo;

void main() {
    vec3 n = normalize(vNormal);
    float ndotl = max(dot(n, normalize(-ubo.lightDir.xyz)), 0.0);
    vec3 base = vec3(0.45, 0.62, 0.85);
    vec3 color = base * (0.25 + 0.75 * ndotl);
    outColor = vec4(color, 1.0);
}
