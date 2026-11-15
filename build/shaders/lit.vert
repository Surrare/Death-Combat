#version 330 core
// Death Combat — Forward Lit Vertex Shader (World-space transform, normal matrix, light-space shadow coords)
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;

uniform mat4 uModel, uView, uProj, uLightVP;

out vec3 vWorld;
out vec3 vNormal;
out vec3 vColor;
out vec4 vShadow;

void main() {
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vNormal = normalize(transpose(inverse(mat3(uModel))) * aNormal);
    vColor = aColor;
    vShadow = uLightVP * w;
    gl_Position = uProj * uView * w;
}
