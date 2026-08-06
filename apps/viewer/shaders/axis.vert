#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
    vec4 albedoColor;
} ubo;

layout(location = 0) out vec3 vColor;

void main() {
    vColor = inColor;
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
