#version 450
layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
    mat4 model;
    vec4 lightDir;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);
}
