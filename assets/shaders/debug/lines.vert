#version 460 core

// Отладочные линии: хитбоксы, пушбоксы, пол, стены (engine/gfx/debug_draw.cpp).
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) uniform mat4 uViewProjection;

out vec4 vColor;

void main() {
    vColor = inColor;
    gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
