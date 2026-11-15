#version 330 core
// Death Combat — Forward Lit Fragment Shader
// Implements: Hemisphere Ambient + Wrap-Diffuse + Cook-Torrance (GGX) or Blinn-Phong Specular + 3x3 PCF Shadow Mapping
// + Dynamic Point Lights + Fresnel Rim Lighting + Procedural Patterns with Normal-Mapped Surface Textures + Exponential-Squared Fog
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
uniform int uBrdf;          // 1 = Cook-Torrance GGX, 0 = Blinn-Phong (F2 mode BLINN-PHONG)
uniform int uTexEnabled;    // 1 = normal-mapped surface textures, 0 = flat procedural patterns (F2 mode NO TEXTURES)
uniform sampler2D uTexFloor;   // RGB tangent-space normal, A detail multiplier; one period = one tile
uniform sampler2D uTexBrick;   // one period = one brick cell
uniform sampler2D uTexMetal;   // one period = one panel cell

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

// Samples a surface texture and perturbs the normal with a tangent frame built from screen-space derivatives
// of the continuous coordinate uvC (no per-vertex tangents). uvS is the coordinate actually sampled (it may carry the brick stagger).
void applyTexture(sampler2D tex, vec2 uvC, vec2 uvS, inout vec3 base, inout vec3 n) {
    if (uTexEnabled == 0) return;
    vec3 dp1 = dFdx(vWorld), dp2 = dFdy(vWorld);
    vec2 duv1 = dFdx(uvC), duv2 = dFdy(uvC);
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) < 1e-9) return;                       // degenerate mapping (face seen edge-on or constant along one axis)
    vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invMax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
    vec4 t = textureGrad(tex, uvS, duv1, duv2);
    float fade = 1.0 - smoothstep(0.30, 0.80, max(length(duv1), length(duv2)));   // far surfaces fall back to the flat pattern
    vec3 tn = vec3((t.xy * 2.0 - 1.0) * fade, t.z * 2.0 - 1.0 + (1.0 - fade));
    n = normalize(mat3(T * invMax, B * invMax, n) * tn);
    base *= mix(1.0, t.a * 1.12, fade);
}

// Procedural pattern colour, optional normal map, and material parameters for the specular term
void surface(inout vec3 base, inout vec3 n, out float rough, out float metal) {
    rough = 0.5; metal = 0.0;
    if (uPattern == 1) { // floor tiles
        vec2 c = floor(vWorld.xz * 0.5);
        float h = hash(c);
        vec2 f = fract(vWorld.xz * 0.5);
        float edge = gridLine(f, vec2(0.03), fwidth(vWorld.xz * 0.5));
        base = base * (0.85 + 0.3 * h) * (0.55 + 0.45 * edge);
        applyTexture(uTexFloor, vWorld.xz * 0.5, vWorld.xz * 0.5, base, n);
        rough = 0.42;
    } else if (uPattern == 2) { // bricks on walls
        float row = floor(vWorld.y * 3.0);
        float alongC = (abs(vNormal.x) > 0.5 ? vWorld.z : vWorld.x) * 1.5;
        float along = alongC + mod(row, 2.0) * 0.5;
        vec2 c = vec2(floor(along), row);
        float h = hash(c);
        vec2 f = vec2(fract(along), fract(vWorld.y * 3.0));
        float mortar = gridLine(f, vec2(0.05, 0.08), vec2(fwidth(along), fwidth(vWorld.y * 3.0)));
        base = base * (0.8 + 0.4 * h) * (0.5 + 0.5 * mortar);
        applyTexture(uTexBrick, vec2(alongC, vWorld.y * 3.0), vec2(along, vWorld.y * 3.0), base, n);
        rough = 0.85;
    } else if (uPattern == 3) { // metal panels
        vec2 uv = (abs(vNormal.y) > 0.5) ? vWorld.xz : (abs(vNormal.x) > 0.5 ? vWorld.zy : vWorld.xy);
        vec2 c = floor(uv * 0.75);
        vec2 f = fract(uv * 0.75);
        float groove = gridLine(f, vec2(0.04), fwidth(uv * 0.75));
        base = base * (0.9 + 0.2 * hash(c)) * (0.6 + 0.4 * groove);
        applyTexture(uTexMetal, uv * 0.75, uv * 0.75, base, n);
        rough = 0.32; metal = 0.85;
    }
}

// Cook-Torrance specular (GGX distribution, Smith-Schlick geometry, Schlick Fresnel), already multiplied by N.L
vec3 specGGX(vec3 n, vec3 v, vec3 l, float rough, float metal, vec3 base) {
    vec3 h = normalize(l + v);
    float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 1e-3);
    float nh = max(dot(n, h), 0.0), vh = max(dot(v, h), 0.0);
    float r2 = rough * rough;
    float a2 = r2 * r2;
    float dd = nh * nh * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159265 * dd * dd);
    float k = (rough + 1.0) * (rough + 1.0) * 0.125;
    float G = (nl / (nl * (1.0 - k) + k)) * (nv / (nv * (1.0 - k) + k));
    vec3 F0 = mix(vec3(0.04), base, metal);
    float m1 = 1.0 - vh, m2 = m1 * m1;
    vec3 F = F0 + (1.0 - F0) * (m2 * m2 * m1);
    return D * G * F / (4.0 * nv * max(nl, 1e-3)) * nl * 0.45;
}

void main() {
    vec3 ng = normalize(vNormal);            // geometric normal (shadow bias)
    vec3 n = ng;
    vec3 base = vColor * uColor.rgb;
    float rough, metal;
    surface(base, n, rough, metal);
    vec3 v = normalize(uEye - vWorld);

    // Hemisphere ambient
    float hemi = 0.5 + 0.5 * n.y;
    vec3 col = base * uAmbient * mix(0.6, 1.2, hemi);

    // Directional sun / moon light with Blinn-Phong half-vector specular & PCF shadow
    vec3 l = normalize(-uSunDir);
    float ndl0 = dot(n, l);
    float ndl = mix(max(ndl0, 0.0), pow(ndl0 * 0.5 + 0.5, 2.0), 0.35);
    float sh = shadowFactor(ng);
    float specMul = uPattern == 1 ? 0.25 : 1.0;
    vec3 diffMul = vec3(uBrdf == 1 ? 1.0 - 0.8 * metal : 1.0);
    vec3 sunSpec;
    if (uBrdf == 1) {
        sunSpec = specGGX(n, v, l, rough, metal, base);
    } else {
        vec3 h = normalize(l + v);
        sunSpec = vec3(pow(max(dot(n, h), 0.0), 32.0) * 0.25 * specMul);
    }
    col += (base * ndl * diffMul + sunSpec) * uSunColor * sh;

    // Dynamic point lights with quadratic attenuation
    for (int i = 0; i < uNumLights; i++) {
        vec3 d = uLightPos[i].xyz - vWorld;
        float dist = length(d);
        float att = clamp(1.0 - dist / uLightPos[i].w, 0.0, 1.0);
        att *= att;
        if (att <= 0.0) continue;   // light out of range: skip the specular and diffuse work
        vec3 ld = d / max(dist, 0.001);
        float pdl0 = dot(n, ld);
        float pdl = mix(max(pdl0, 0.0), pow(pdl0 * 0.5 + 0.5, 2.0), 0.3);
        vec3 pSpec;
        if (uBrdf == 1) {
            pSpec = specGGX(n, v, ld, rough, metal, base);
        } else {
            vec3 ph = normalize(ld + v);
            pSpec = vec3(pow(max(dot(n, ph), 0.0), 24.0) * 0.3 * specMul);
        }
        col += (base * pdl * diffMul + pSpec) * uLightColor[i] * att;
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
