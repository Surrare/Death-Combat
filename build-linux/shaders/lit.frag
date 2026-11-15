#version 330 core
// Death Combat — Forward Lit Fragment Shader
// Implements: Hemisphere Ambient + Wrap-Diffuse + Blinn-Phong Specular + 3x3 PCF Shadow Mapping
// + Dynamic Point Lights + Fresnel Rim Lighting + Procedural World-Space Patterns + Exponential-Squared Fog
in vec3 vWorld;
in vec3 vNormal;
in vec3 vColor;
in vec4 vShadow;

out vec4 FragColor;

uniform vec4 uColor;
uniform float uEmissive;
uniform int uPattern;
uniform vec3 uSunDir, uSunColor;
uniform float uAmbient;
uniform vec3 uEye;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform vec3 uRimColor;
uniform int uNumLights;
uniform vec4 uLightPos[16];
uniform vec3 uLightColor[16];
uniform sampler2DShadow uShadowMap;
uniform int uShadowEnabled;

// 0 on a grid line, 1 inside a cell; lines fade out with distance so they do not shimmer when the camera moves
float gridLine(vec2 f, vec2 width, vec2 fw) {
    vec2 e = smoothstep(width, width + fw * 1.5, f);
    float line = e.x * e.y;
    return mix(line, 0.94, clamp(max(fw.x, fw.y) * 6.0, 0.0, 1.0));
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// 3x3 Percentage-Closer Filtering (PCF) with slope-scaled depth bias
float shadowFactor(vec3 n) {
    if (uShadowEnabled == 0) return 1.0;
    vec3 p = vShadow.xyz / vShadow.w * 0.5 + 0.5;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;
    float bias = max(0.0025 * (1.0 - dot(n, -uSunDir)), 0.0008);
    float s = 0.0;
    vec2 texel = vec2(1.0 / 2048.0);
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            s += texture(uShadowMap, vec3(p.xy + vec2(x, y) * texel, p.z - bias));
    return s / 9.0;
}

vec3 patternColor(vec3 base) {
    if (uPattern == 1) { // floor tiles
        vec2 c = floor(vWorld.xz * 0.5);
        float h = hash(c);
        vec2 f = fract(vWorld.xz * 0.5);
        float edge = gridLine(f, vec2(0.03), fwidth(vWorld.xz * 0.5));
        return base * (0.85 + 0.3 * h) * (0.55 + 0.45 * edge);
    } else if (uPattern == 2) { // bricks on walls
        float row = floor(vWorld.y * 3.0);
        float along = (abs(vNormal.x) > 0.5 ? vWorld.z : vWorld.x) * 1.5 + mod(row, 2.0) * 0.5;
        vec2 c = vec2(floor(along), row);
        float h = hash(c);
        vec2 f = vec2(fract(along), fract(vWorld.y * 3.0));
        float mortar = gridLine(f, vec2(0.05, 0.08), vec2(fwidth(along), fwidth(vWorld.y * 3.0)));
        return base * (0.8 + 0.4 * h) * (0.5 + 0.5 * mortar);
    } else if (uPattern == 3) { // metal panels
        vec2 uv = (abs(vNormal.y) > 0.5) ? vWorld.xz : (abs(vNormal.x) > 0.5 ? vWorld.zy : vWorld.xy);
        vec2 c = floor(uv * 0.75);
        vec2 f = fract(uv * 0.75);
        float groove = gridLine(f, vec2(0.04), fwidth(uv * 0.75));
        return base * (0.9 + 0.2 * hash(c)) * (0.6 + 0.4 * groove);
    }
    return base;
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 base = patternColor(vColor * uColor.rgb);
    vec3 v = normalize(uEye - vWorld);

    // Hemisphere ambient
    float hemi = 0.5 + 0.5 * n.y;
    vec3 col = base * uAmbient * mix(0.6, 1.2, hemi);

    // Directional sun / moon light with Blinn-Phong half-vector specular & PCF shadow
    vec3 l = normalize(-uSunDir);
    float ndl0 = dot(n, l);
    float ndl = mix(max(ndl0, 0.0), pow(ndl0 * 0.5 + 0.5, 2.0), 0.35);
    float sh = shadowFactor(n);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), 32.0) * 0.25;
    float specMul = uPattern == 1 ? 0.25 : 1.0;
    col += (base * ndl + vec3(spec * specMul)) * uSunColor * sh;

    // Dynamic point lights with quadratic attenuation
    for (int i = 0; i < uNumLights; i++) {
        vec3 d = uLightPos[i].xyz - vWorld;
        float dist = length(d);
        float att = clamp(1.0 - dist / uLightPos[i].w, 0.0, 1.0);
        att *= att;
        vec3 ld = d / max(dist, 0.001);
        float pdl0 = dot(n, ld);
        float pdl = mix(max(pdl0, 0.0), pow(pdl0 * 0.5 + 0.5, 2.0), 0.3);
        vec3 ph = normalize(ld + v);
        float ps = pow(max(dot(n, ph), 0.0), 24.0) * 0.3;
        col += (base * pdl + vec3(ps * specMul)) * uLightColor[i] * att;
    }

    // Fresnel rim light on characters and props
    float rim = pow(1.0 - max(dot(n, v), 0.0), 2.6);
    float rimMul = uPattern == 0 ? 1.0 : 0.15;
    col += rim * rimMul * (base * 0.35 + uRimColor * 0.22) * (0.35 + 0.65 * smoothstep(-0.3, 0.6, n.y * 0.4 + 0.4));
    col = mix(col, uColor.rgb * vColor, uEmissive);

    // Exponential-squared distance fog
    float dist = length(uEye - vWorld);
    float fog = 1.0 - exp(-uFogDensity * dist * dist);
    col = mix(col, uFogColor, clamp(fog, 0.0, 1.0));
    FragColor = vec4(col, uColor.a);
}
