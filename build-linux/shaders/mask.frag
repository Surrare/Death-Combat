#version 330 core
out vec4 FragColor;
uniform vec2 uCenter, uNormal; uniform vec3 uClear;
void main(){
    if (dot(gl_FragCoord.xy - uCenter, uNormal) <= 0.0) discard;
    FragColor = vec4(uClear, 1.0);
    gl_FragDepth = 1.0;
}
