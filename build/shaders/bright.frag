#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex; uniform float uThreshold;
void main(){
    vec3 c = texture(uTex, vUV).rgb;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - uThreshold, 0.0) / max(l, 1e-4);
    FragColor = vec4(c * k, 1.0);
}
