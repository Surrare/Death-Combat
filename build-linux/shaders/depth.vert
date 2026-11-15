#version 330 core
// Death Combat — Shadow Map Depth Pass Vertex Shader
layout(location=0) in vec3 aPos;
uniform mat4 uModel, uLightVP;
void main() {
    gl_Position = uLightVP * uModel * vec4(aPos, 1.0);
}
