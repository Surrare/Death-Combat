#version 330 core
in vec2 vUV; in vec4 vColor; out vec4 FragColor;
uniform sampler2D uFont;
void main(){
    float distance = texture(uFont, vUV).r;
    float edge = 128.0 / 255.0;
    float aa = clamp(0.5 * fwidth(distance), 1.0 / 255.0, 0.25);
    float a = vUV.x < -0.5 ? 1.0 : smoothstep(edge - aa, edge + aa, distance);
    FragColor = vec4(vColor.rgb, vColor.a * a);
}
