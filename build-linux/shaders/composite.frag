#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene, uBloom, uDepth;
uniform vec2 uTexel; uniform float uNear, uFar, uBloomStrength, uOutline;
float linDepth(float d){ float z = d * 2.0 - 1.0; return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear)); }
float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main(){
    vec3 col = texture(uScene, vUV).rgb;

    // ink outline from depth discontinuities (silhouettes only; relative so distance does not matter)
    float d0raw = texture(uDepth, vUV).r;
    if (uOutline > 0.0 && d0raw < 0.9999) {
        float d0 = linDepth(d0raw);
        vec2 o = uTexel * max(1.0, uTexel.y * 0.0 + 1.0);
        float e = 0.0;
        e = max(e, abs(linDepth(texture(uDepth, vUV + vec2(o.x, 0)).r) - d0));
        e = max(e, abs(linDepth(texture(uDepth, vUV - vec2(o.x, 0)).r) - d0));
        e = max(e, abs(linDepth(texture(uDepth, vUV + vec2(0, o.y)).r) - d0));
        e = max(e, abs(linDepth(texture(uDepth, vUV - vec2(0, o.y)).r) - d0));
        float edge = smoothstep(0.035, 0.11, e / d0) * (1.0 - smoothstep(10.0, 22.0, d0));   // far geometry (floor edge, distant walls) stays clean
        col = mix(col, vec3(0.012, 0.008, 0.02), edge * uOutline);
    }

    // bloom
    if (uBloomStrength > 0.0) col += texture(uBloom, vUV).rgb * uBloomStrength;

    // soft shoulder: leave the palette alone below 0.8, roll highlights off smoothly instead of clipping
    const float knee = 0.8;
    vec3 over = max(col - knee, 0.0);
    col = min(col, vec3(knee)) + (1.0 - knee) * (1.0 - exp(-over / (1.0 - knee)));

    // grade: a touch more saturation and contrast
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(l), col, 1.14);
    col = mix(col, col * col * (3.0 - 2.0 * col), 0.28);

    // vignette
    vec2 q = vUV - 0.5;
    col *= clamp(1.0 - dot(q, q) * 0.95, 0.0, 1.0);

    col += (hash(gl_FragCoord.xy) - 0.5) / 255.0;   // dither: hides banding in dark gradients
    FragColor = vec4(col, 1.0);
}
