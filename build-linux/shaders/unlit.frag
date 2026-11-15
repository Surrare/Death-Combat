#version 330 core
// Death Combat — Unlit Radial Soft-Particle Fragment Shader
in vec4 vColor;
in vec2 vUV;
out vec4 FragColor;
void main() {
    float d = length(vUV);
    float soft = 1.0 - smoothstep(0.15, 1.0, d);
    soft = soft * soft;
    FragColor = vec4(vColor.rgb, vColor.a * soft);
}
