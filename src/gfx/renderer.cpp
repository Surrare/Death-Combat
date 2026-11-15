#include "gfx/renderer.h"
#include "core/font.h"
#include "gfx/textures.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace dc {
namespace {
constexpr float TEXT_CAP_HEIGHT = 9.0f;
FontFace textFace(float scale) { return scale >= 2.4f ? FontFace::Display : FontFace::Body; }
}

// ---------------------------------------------------------------- shaders ----
// Embedded fallback copies of the files in shaders/. They are used only when the files cannot be found or fail to
// compile, and are generated from the files (tools/embed_shaders.py), so edit the files in shaders/, not these strings.
static const char* LIT_VS = R"GLSL(#version 330 core
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
)GLSL";

static const char* LIT_FS = R"GLSL(#version 330 core
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
)GLSL";

static const char* DEPTH_VS = R"GLSL(#version 330 core
// Death Combat — Shadow Map Depth Pass Vertex Shader
layout(location=0) in vec3 aPos;
uniform mat4 uModel, uLightVP;
void main() {
    gl_Position = uLightVP * uModel * vec4(aPos, 1.0);
}
)GLSL";

static const char* DEPTH_FS = R"GLSL(#version 330 core
// Death Combat — Shadow Map Depth Pass Fragment Shader (depth written automatically)
void main() {}
)GLSL";

static const char* UNLIT_VS = R"GLSL(#version 330 core
// Death Combat — Unlit Particle & Billboard Vertex Shader
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in vec2 aUV;
uniform mat4 uViewProj;
out vec4 vColor;
out vec2 vUV;
void main() {
    vColor = aColor;
    vUV = aUV;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)GLSL";

static const char* UNLIT_FS = R"GLSL(#version 330 core
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
)GLSL";

static const char* UI_VS = R"GLSL(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV; out vec4 vColor;
void main(){
    vUV = aUV; vColor = aColor;
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

static const char* UI_FS = R"GLSL(#version 330 core
in vec2 vUV; in vec4 vColor; out vec4 FragColor;
uniform sampler2D uFont;
void main(){
    float distance = texture(uFont, vUV).r;
    float edge = 128.0 / 255.0;
    float aa = clamp(0.5 * fwidth(distance), 1.0 / 255.0, 0.25);
    float a = vUV.x < -0.5 ? 1.0 : smoothstep(edge - aa, edge + aa, distance);
    FragColor = vec4(vColor.rgb, vColor.a * a);
}
)GLSL";

static const char* POST_VS = R"GLSL(#version 330 core
out vec2 vUV;
void main(){
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

static const char* BRIGHT_FS = R"GLSL(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex; uniform float uThreshold;
void main(){
    vec3 c = texture(uTex, vUV).rgb;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - uThreshold, 0.0) / max(l, 1e-4);
    FragColor = vec4(c * k, 1.0);
}
)GLSL";

static const char* BLUR_FS = R"GLSL(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex; uniform vec2 uDir;
void main(){
    const float w[5] = float[5](0.2270270, 0.1945946, 0.1216216, 0.0540541, 0.0162162);
    vec3 c = texture(uTex, vUV).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uTex, vUV + uDir * float(i)).rgb * w[i];
        c += texture(uTex, vUV - uDir * float(i)).rgb * w[i];
    }
    FragColor = vec4(c, 1.0);
}
)GLSL";

static const char* COMPOSITE_FS = R"GLSL(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uScene, uBloom, uDepth, uAo;
uniform vec2 uTexel, uAoTexel; uniform float uNear, uFar, uBloomStrength, uOutline, uAoAmount;
float linDepth(float d){ float z = d * 2.0 - 1.0; return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear)); }
float hash(vec2 p){ return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main(){
    vec3 col = texture(uScene, vUV).rgb;

    // screen-space ambient occlusion: 3x3 box blur of the half-resolution map hides the sampling noise
    if (uAoAmount > 0.0) {
        float ao = 0.0;
        for (int x = -1; x <= 1; x++)
            for (int y = -1; y <= 1; y++)
                ao += texture(uAo, vUV + vec2(x, y) * uAoTexel).r;
        col *= mix(1.0, ao / 9.0, uAoAmount);
    }

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
)GLSL";

static const char* MASK_FS = R"GLSL(#version 330 core
out vec4 FragColor;
uniform vec2 uCenter, uNormal; uniform vec3 uClear;
void main(){
    if (dot(gl_FragCoord.xy - uCenter, uNormal) <= 0.0) discard;
    FragColor = vec4(uClear, 1.0);
    gl_FragDepth = 1.0;
}
)GLSL";

static const char* SSAO_FS = R"GLSL(#version 330 core
// Death Combat — Screen-Space Ambient Occlusion (half resolution, depth only).
// Reconstructs view-space position and normal from the resolved depth buffer, tests 16 hemisphere samples
// against the depth buffer, and writes an occlusion factor (1 = open, 0 = fully occluded) to the red channel.
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uDepth;
uniform mat4 uProj;
uniform vec2 uTanHalf;          // (aspect * tan(fov/2), tan(fov/2))
uniform float uNear, uFar, uRadius, uStrength;

float linDepth(float d) { float z = d * 2.0 - 1.0; return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear)); }
// Integer texel fetches: sampling a NEAREST depth texture at exact texel boundaries (which is where half-resolution
// pixel centres fall) picks rows inconsistently and shows up as horizontal bands.
vec3 viewPos(ivec2 ip) {
    ivec2 sz = textureSize(uDepth, 0);
    ip = clamp(ip, ivec2(0), sz - 1);
    float z = linDepth(texelFetch(uDepth, ip, 0).r);
    vec2 uv = (vec2(ip) + 0.5) / vec2(sz);
    return vec3((uv * 2.0 - 1.0) * uTanHalf * z, -z);
}
// interleaved gradient noise: well distributed per pixel, no row or column correlation
float ign(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

void main() {
    ivec2 sz = textureSize(uDepth, 0);
    ivec2 ip = clamp(ivec2(gl_FragCoord.xy) * 2, ivec2(0), sz - 1);   // the full-resolution pixel under this half-res pixel
    float d0 = texelFetch(uDepth, ip, 0).r;
    if (d0 >= 0.9999) { FragColor = vec4(1.0); return; }      // sky / background
    vec3 p = viewPos(ip);
    // normal from central differences; one-sided differences only where the second difference shows a depth edge
    vec3 pr = viewPos(ip + ivec2(1, 0)), pl = viewPos(ip - ivec2(1, 0));
    vec3 pu = viewPos(ip + ivec2(0, 1)), pd = viewPos(ip - ivec2(0, 1));
    float ex = abs(pr.z + pl.z - 2.0 * p.z), ey = abs(pu.z + pd.z - 2.0 * p.z);
    float tx = 0.15 * max(abs(pr.z - p.z), abs(p.z - pl.z)) + 0.002;
    float ty = 0.15 * max(abs(pu.z - p.z), abs(p.z - pd.z)) + 0.002;
    vec3 dx = ex < tx ? (pr - pl) : (abs(pr.z - p.z) < abs(p.z - pl.z) ? pr - p : p - pl);
    vec3 dy = ey < ty ? (pu - pd) : (abs(pu.z - p.z) < abs(p.z - pd.z) ? pu - p : p - pd);
    vec3 n = normalize(cross(dx, dy));
    if (dot(n, -p) < 0.0) n = -n;

    float ang = 6.2831853 * ign(gl_FragCoord.xy);
    vec3 rvec = vec3(cos(ang), sin(ang), 0.0);
    vec3 t = normalize(rvec - n * dot(rvec, n));
    vec3 b = cross(n, t);

    float occ = 0.0;
    const int N = 16;
    for (int i = 0; i < N; i++) {
        float fi = (float(i) + 0.5) / float(N);
        float phi = float(i) * 2.399963;                       // golden angle spiral over the hemisphere
        float kz = mix(0.25, 1.0, fi);                         // stay off the tangent plane so flat surfaces do not self-occlude
        float r = sqrt(1.0 - kz * kz);
        vec3 k = vec3(r * cos(phi), r * sin(phi), kz);
        k *= mix(0.25, 1.0, fi * fi);                          // more samples close to the surface point
        vec3 sp = p + (t * k.x + b * k.y + n * k.z) * uRadius;
        vec4 clip = uProj * vec4(sp, 1.0);
        vec2 suv = clip.xy / clip.w * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        float sceneZ = linDepth(texelFetch(uDepth, clamp(ivec2(suv * vec2(sz)), ivec2(0), sz - 1), 0).r);   // positive distance of the visible surface
        float sampleZ = -sp.z;
        float range = smoothstep(0.0, 1.0, uRadius / max(abs(p.z + sceneZ), 1e-4));   // ignore far-away occluders
        float bias = 0.03 + 0.015 * sampleZ;                    // depth precision falls off with distance
        occ += (sceneZ < sampleZ - bias ? 1.0 : 0.0) * range;
    }
    float ao = 1.0 - occ / float(N);
    ao = mix(1.0, ao, smoothstep(0.7, 2.2, -p.z));          // very close surfaces: the sample disc covers too many pixels and turns to noise
    FragColor = vec4(pow(clamp(ao, 0.0, 1.0), 1.0 + uStrength), 0.0, 0.0, 1.0);
}
)GLSL";

// ------------------------------------------------------------------- init ----
Renderer::~Renderer() {
    destroyTargets();
    if (postVao_) glDeleteVertexArrays(1, &postVao_);
    if (shadowFbo_) glDeleteFramebuffers(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    if (unlitVao_) glDeleteVertexArrays(1, &unlitVao_);
    if (unlitVbo_) glDeleteBuffers(1, &unlitVbo_);
    if (uiVao_) glDeleteVertexArrays(1, &uiVao_);
    if (uiVbo_) glDeleteBuffers(1, &uiVbo_);
    if (fontTex_) glDeleteTextures(1, &fontTex_);
    for (GLuint t : {texFloor_, texBrick_, texMetal_}) if (t) glDeleteTextures(1, &t);
}

bool Renderer::reloadShaders() {
    if (!lit_.compileFromFiles("lit.vert", "lit.frag", LIT_VS, LIT_FS, "lit")) return false;
    if (!depth_.compileFromFiles("depth.vert", "depth.frag", DEPTH_VS, DEPTH_FS, "depth")) return false;
    if (!unlit_.compileFromFiles("unlit.vert", "unlit.frag", UNLIT_VS, UNLIT_FS, "unlit")) return false;
    if (!ui2d_.compileFromFiles("ui.vert", "ui.frag", UI_VS, UI_FS, "ui")) return false;
    bool postShaders = bright_.compileFromFiles("post.vert", "bright.frag", POST_VS, BRIGHT_FS, "bright") &&
                       blur_.compileFromFiles("post.vert", "blur.frag", POST_VS, BLUR_FS, "blur") &&
                       composite_.compileFromFiles("post.vert", "composite.frag", POST_VS, COMPOSITE_FS, "composite");
    if (!mask_.compileFromFiles("post.vert", "mask.frag", POST_VS, MASK_FS, "split mask")) return false;
    ssaoOk_ = ssao_.compileFromFiles("post.vert", "ssao.frag", POST_VS, SSAO_FS, "ssao");
    if (!ssaoOk_) std::fprintf(stderr, "[renderer] SSAO shader unavailable, ambient occlusion disabled\n");
    if (!postShaders) targetW_ = targetH_ = -1;
    return true;
}

bool Renderer::init() {
    if (!reloadShaders()) return false;
    glGenVertexArrays(1, &postVao_);

    // generated surface textures (normal map + detail) for floor tiles, bricks and metal panels
    texFloor_ = uploadTexture(generateSurfaceTexture(SurfaceTexture::FloorTile), "floor tile");
    texBrick_ = uploadTexture(generateSurfaceTexture(SurfaceTexture::Brick), "brick");
    texMetal_ = uploadTexture(generateSurfaceTexture(SurfaceTexture::MetalPanel), "metal panel");

    // shadow map
    glGenFramebuffers(1, &shadowFbo_);
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize_, shadowSize_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    shadowOk_ = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!shadowOk_)
        std::fprintf(stderr, "[renderer] shadow framebuffer incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // unlit batch
    glGenVertexArrays(1, &unlitVao_);
    glGenBuffers(1, &unlitVbo_);
    glBindVertexArray(unlitVao_);
    glBindBuffer(GL_ARRAY_BUFFER, unlitVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(UnlitVertex), (void*)offsetof(UnlitVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UnlitVertex), (void*)offsetof(UnlitVertex, col));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(UnlitVertex), (void*)offsetof(UnlitVertex, u));
    glBindVertexArray(0);

    // ui batch
    glGenVertexArrays(1, &uiVao_);
    glGenBuffers(1, &uiVbo_);
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)offsetof(UiVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)offsetof(UiVertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UiVertex), (void*)offsetof(UiVertex, col));
    glBindVertexArray(0);

    if (!buildFontAtlas()) return false;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    return true;
}

bool Renderer::buildFontAtlas() {
    if (!font_.build()) {
        std::fprintf(stderr, "Failed to build the embedded TrueType font atlas\n");
        return false;
    }
    glGenTextures(1, &fontTex_);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FontAtlas::WIDTH, FontAtlas::HEIGHT, 0, GL_RED, GL_UNSIGNED_BYTE, font_.pixels().data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return true;
}

// ------------------------------------------------------------- submission ----
void Renderer::beginFrame(const Camera& cam, const Color& clear) {
    cam_ = cam;
    clear_ = clear;
    float aspect = (float)std::max(width_, 1) / (float)std::max(height_, 1);
    proj_ = Mat4::perspective(radians(cam.fovDeg), aspect, cam.znear, cam.zfar);
    view_ = Mat4::lookAt(cam.eye, cam.target, Vec3(0, 1, 0));
    Vec3 f = normalize(cam.target - cam.eye);
    camRight_ = normalize(cross(f, Vec3(0, 1, 0)));
    camUp_ = cross(camRight_, f);
    opaque_.clear(); transparent_.clear();
    unlitAlpha_.clear(); unlitAdd_.clear(); ui_.clear();
    pointLights_.clear();
    split_ = false;
}

void Renderer::setSecondView(const Camera& cam, const Vec2& splitNormal) {
    split_ = true;
    camB_ = cam;
    float aspect = (float)std::max(width_, 1) / (float)std::max(height_, 1);
    projB_ = Mat4::perspective(radians(cam.fovDeg), aspect, cam.znear, cam.zfar);
    viewB_ = Mat4::lookAt(cam.eye, cam.target, Vec3(0, 1, 0));
    float l = std::sqrt(splitNormal.x * splitNormal.x + splitNormal.y * splitNormal.y);
    splitN_ = l > 1e-5f ? Vec2(splitNormal.x / l, splitNormal.y / l) : Vec2(1, 0);
}

bool Renderer::projectToScreen(const Vec3& p, float& sx, float& sy) const {
    const float w = (float)width_, h = (float)height_;
    auto toScreen = [&](const Mat4& pv, float& x, float& y) {
        Vec4 c = pv * Vec4(p, 1.0f);
        if (c.w <= 0.001f) return false;
        x = (c.x / c.w * 0.5f + 0.5f) * w;
        y = (1.0f - (c.y / c.w * 0.5f + 0.5f)) * h;
        return true;
    };
    auto inB = [&](float x, float y) { return (x - w * 0.5f) * splitN_.x + (y - h * 0.5f) * splitN_.y > 0.0f; };
    float ax = 0, ay = 0;
    bool okA = toScreen(proj_ * view_, ax, ay);
    if (!split_) { sx = ax; sy = ay; return okA; }
    if (okA && !inB(ax, ay)) { sx = ax; sy = ay; return true; }
    float bx, by;
    if (toScreen(projB_ * viewB_, bx, by) && inB(bx, by)) { sx = bx; sy = by; return true; }
    return false;
}

void Renderer::setDirectionalLight(const Vec3& dir, const Vec3& color, float ambient) {
    Vec3 d = normalize(dir);
    if (std::fabs(lightYawDeg_) > 0.001f || std::fabs(lightPitchDeg_) > 0.001f) {
        float az = std::atan2(d.z, d.x) + radians(lightYawDeg_);
        float el = clampf(std::asin(clampf(d.y, -0.98f, -0.15f)) + radians(lightPitchDeg_), -1.46f, -0.22f);
        float cEl = std::cos(el);
        d = normalize(Vec3(std::cos(az) * cEl, std::sin(el), std::sin(az) * cEl));
    }
    sunDir_ = d;
    sunColor_ = color;
    ambient_ = ambient;
}

void Renderer::draw(const Mesh& mesh, const Mat4& model, const Color& color, float emissive, int pattern, bool castShadow) {
    DrawCmd c{&mesh, model, color, emissive, pattern, castShadow};
    if (color.a < 0.999f) transparent_.push_back(c); else opaque_.push_back(c);
}

void Renderer::drawUnlitTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Color& col, Blend blend) {
    auto& v = blend == Blend::Additive ? unlitAdd_ : unlitAlpha_;
    v.push_back({a, col, 0, 0}); v.push_back({b, col, 0, 0}); v.push_back({c, col, 0, 0});
}

void Renderer::drawBillboard(const Vec3& center, float size, const Color& col, Blend blend, float rotation) {
    float cs = std::cos(rotation), sn = std::sin(rotation);
    Vec3 r = (camRight_ * cs + camUp_ * sn) * (size * 0.5f);
    Vec3 u = (camUp_ * cs - camRight_ * sn) * (size * 0.5f);
    Vec3 p0 = center - r - u, p1 = center + r - u, p2 = center + r + u, p3 = center - r + u;
    auto& v = blend == Blend::Additive ? unlitAdd_ : unlitAlpha_;
    v.push_back({p0, col, -1, -1}); v.push_back({p1, col, 1, -1}); v.push_back({p2, col, 1, 1});
    v.push_back({p0, col, -1, -1}); v.push_back({p2, col, 1, 1}); v.push_back({p3, col, -1, 1});
}

void Renderer::drawLine3D(const Vec3& a, const Vec3& b, float thickness, const Color& col, Blend blend) {
    Vec3 d = b - a;
    Vec3 viewDir = normalize(cam_.target - cam_.eye);
    Vec3 side = normalize(cross(d, viewDir)) * (thickness * 0.5f);
    drawUnlitTriangle(a - side, b - side, b + side, col, blend);
    drawUnlitTriangle(a - side, b + side, a + side, col, blend);
}

void Renderer::rect(float x, float y, float w, float h, const Color& col) {
    UiVertex v0{x, y, -2, -2, col}, v1{x + w, y, -2, -2, col}, v2{x + w, y + h, -2, -2, col}, v3{x, y + h, -2, -2, col};
    ui_.push_back(v0); ui_.push_back(v1); ui_.push_back(v2);
    ui_.push_back(v0); ui_.push_back(v2); ui_.push_back(v3);
}

void Renderer::rectOutline(float x, float y, float w, float h, float t, const Color& col) {
    rect(x, y, w, t, col); rect(x, y + h - t, w, t, col);
    rect(x, y, t, h, col); rect(x + w - t, y, t, h, col);
}

void Renderer::triangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color& col) {
    ui_.push_back({x0, y0, -2, -2, col}); ui_.push_back({x1, y1, -2, -2, col}); ui_.push_back({x2, y2, -2, -2, col});
}

void Renderer::line(float x0, float y0, float x1, float y1, float thickness, const Color& col) {
    float dx = x1 - x0, dy = y1 - y0, l = std::sqrt(dx * dx + dy * dy);
    if (l < 1e-4f) return;
    float nx = -dy / l * thickness * 0.5f, ny = dx / l * thickness * 0.5f;
    triangle(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, col);
    triangle(x0 + nx, y0 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, col);
}

void Renderer::text(float x, float y, const std::string& s, float scale, const Color& col) {
    const std::string translated = localizeText(s, language_);
    const TextLayout layout = font_.layout(translated, scale * TEXT_CAP_HEIGHT, textFace(scale));
    for (const GlyphQuad& g : layout.quads) {
        UiVertex v0{x + g.x0, y + g.y0, g.u0, g.v0, col}, v1{x + g.x1, y + g.y0, g.u1, g.v0, col},
                 v2{x + g.x1, y + g.y1, g.u1, g.v1, col}, v3{x + g.x0, y + g.y1, g.u0, g.v1, col};
        ui_.push_back(v0); ui_.push_back(v1); ui_.push_back(v2);
        ui_.push_back(v0); ui_.push_back(v2); ui_.push_back(v3);
    }
}

float Renderer::textWidth(const std::string& s, float scale) const {
    return font_.layout(localizeText(s, language_), scale * TEXT_CAP_HEIGHT, textFace(scale)).width;
}

float Renderer::textLineHeight(float scale) const {
    return font_.lineHeight(scale * TEXT_CAP_HEIGHT, textFace(scale));
}

void Renderer::textCentered(float cx, float y, const std::string& s, float scale, const Color& col) {
    text(cx - textWidth(s, scale) * 0.5f, y, s, scale, col);
}
void Renderer::textRight(float rx, float y, const std::string& s, float scale, const Color& col) {
    text(rx - textWidth(s, scale), y, s, scale, col);
}

// -------------------------------------------------------------- rendering ----
void Renderer::renderShadowPass() {
    // Light view fixed to the world origin, then slid in whole shadow-texel steps to follow the focus.
    // Moving the shadow map continuously makes shadow edges shimmer on the floor whenever the camera / fighters move.
    float dist = shadowExtent_ * 2.5f;
    Vec3 up = std::fabs(sunDir_.y) > 0.95f ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
    Mat4 lvFixed = Mat4::lookAt(sunDir_ * -dist, Vec3(0, 0, 0), up);
    Vec3 c = lvFixed.transformPoint(shadowCenter_);
    float texel = 2.0f * shadowExtent_ / (float)shadowSize_;
    float sx = std::floor(c.x / texel + 0.5f) * texel, sy = std::floor(c.y / texel + 0.5f) * texel;
    Mat4 lv = Mat4::translate(Vec3(-sx, -sy, -(c.z + dist))) * lvFixed;
    Mat4 lp = Mat4::ortho(-shadowExtent_, shadowExtent_, -shadowExtent_, shadowExtent_, 0.1f, shadowExtent_ * 6.0f);
    Mat4 lightVP = lp * lv;

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, shadowSize_, shadowSize_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    depth_.use();
    depth_.set("uLightVP", lightVP);
    for (const DrawCmd& c : opaque_) {
        if (!c.castShadow) continue;
        depth_.set("uModel", c.model);
        c.mesh->draw();
    }
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    lit_.use();
    lit_.set("uLightVP", lightVP);
}

void Renderer::renderScenePass(bool clear) {
    glViewport(0, 0, width_, height_);
    if (clear) {
        glClearColor(clear_.r, clear_.g, clear_.b, 1.0f);
        glClearStencil(0);
        glStencilMask(0xFF);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    lit_.use();
    lit_.set("uView", view_);
    lit_.set("uProj", proj_);
    lit_.set("uSunDir", sunDir_);
    lit_.set("uSunColor", sunColor_);
    lit_.set("uAmbient", ambient_);
    lit_.set("uEye", cam_.eye);
    lit_.set("uFogColor", fogColor_);
    lit_.set("uFogDensity", fogDensity_);
    lit_.set("uRimColor", rimColor_);
    lit_.set("uShadowEnabled", shadowOk_ && debugMode_ != RenderDebugMode::NoShadows ? 1 : 0);
    int n = std::min((int)pointLights_.size(), 16);
    lit_.set("uNumLights", n);
    if (n > 0) {
        Vec4 pos[16]; Vec3 col[16];
        for (int i = 0; i < n; ++i) { pos[i] = Vec4(pointLights_[i].position, pointLights_[i].radius); col[i] = pointLights_[i].color; }
        lit_.setArray("uLightPos", pos, n);
        lit_.setArray("uLightColor", col, n);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    lit_.set("uShadowMap", 0);
    lit_.set("uBrdf", debugMode_ != RenderDebugMode::NoPbr ? 1 : 0);
    lit_.set("uTexEnabled", debugMode_ != RenderDebugMode::NoTextures && texFloor_ && texBrick_ && texMetal_ ? 1 : 0);
    const GLuint surfaceTex[3] = {texFloor_, texBrick_, texMetal_};
    const char* surfaceNames[3] = {"uTexFloor", "uTexBrick", "uTexMetal"};
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE1 + i);
        glBindTexture(GL_TEXTURE_2D, surfaceTex[i]);
        lit_.set(surfaceNames[i], 1 + i);
    }
    glActiveTexture(GL_TEXTURE0);

    auto submit = [&](const DrawCmd& c) {
        lit_.set("uModel", c.model);
        lit_.set("uColor", c.color);
        lit_.set("uEmissive", c.emissive);
        lit_.set("uPattern", c.pattern);
        c.mesh->draw();
    };
    for (const DrawCmd& c : opaque_) submit(c);

    if (!transparent_.empty()) {
        // Depth pre-pass for translucent meshes (Stands) prevents internal box overlap and Z-sort flickering
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        for (const DrawCmd& c : transparent_) submit(c);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        for (const DrawCmd& c : transparent_) submit(c);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

void Renderer::renderUnlit(std::vector<UnlitVertex>& verts, Blend blend) {
    if (verts.empty()) return;
    glEnable(GL_BLEND);
    if (blend == Blend::Additive) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    unlit_.use();
    unlit_.set("uViewProj", proj_ * view_);
    glBindVertexArray(unlitVao_);
    glBindBuffer(GL_ARRAY_BUFFER, unlitVbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(UnlitVertex)), verts.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::renderUi() {
    if (ui_.empty()) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ui2d_.use();
    ui2d_.set("uScreen", Vec2((float)width_, (float)height_));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    ui2d_.set("uFont", 0);
    glBindVertexArray(uiVao_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ui_.size() * sizeof(UiVertex)), ui_.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)ui_.size());
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::destroyTargets(PostTargets& targets) {
    if (targets.msaaFbo) glDeleteFramebuffers(1, &targets.msaaFbo);
    if (targets.resolveFbo) glDeleteFramebuffers(1, &targets.resolveFbo);
    if (targets.msaaColor) glDeleteRenderbuffers(1, &targets.msaaColor);
    if (targets.msaaDepth) glDeleteRenderbuffers(1, &targets.msaaDepth);
    if (targets.aoFbo) glDeleteFramebuffers(1, &targets.aoFbo);
    if (targets.aoTex) glDeleteTextures(1, &targets.aoTex);
    if (targets.sceneTex) glDeleteTextures(1, &targets.sceneTex);
    if (targets.depthTex) glDeleteTextures(1, &targets.depthTex);
    for (int i = 0; i < 2; ++i) {
        if (targets.bloomFbo[i]) glDeleteFramebuffers(1, &targets.bloomFbo[i]);
        if (targets.bloomTex[i]) glDeleteTextures(1, &targets.bloomTex[i]);
    }
    targets = PostTargets{};
}

void Renderer::destroyTargets() {
    destroyTargets(targets_);
    postOk_ = false;
}

void Renderer::createTargets(int w, int h) {
    destroyTargets();
    targetW_ = w;
    targetH_ = h;
    if (w <= 0 || h <= 0) {
        std::fprintf(stderr, "[renderer] invalid post-processing size %dx%d\n", w, h);
        return;
    }

    PostTargets next;
    GLint maxSamples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const int samples = std::max(1, std::min(4, (int)maxSamples));

    auto framebufferComplete = [](const char* name) {
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status == GL_FRAMEBUFFER_COMPLETE) return true;
        std::fprintf(stderr, "[renderer] %s framebuffer incomplete (0x%04x)\n", name, status);
        return false;
    };
    auto makeTex = [](GLuint& tex, GLenum internal, GLenum format, GLenum type, int tw, int th) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, tw, th, 0, format, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    glGenFramebuffers(1, &next.msaaFbo);
    glGenRenderbuffers(1, &next.msaaColor);
    glGenRenderbuffers(1, &next.msaaDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, next.msaaColor);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, next.msaaDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, next.msaaFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, next.msaaColor);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, next.msaaDepth);
    bool ok = framebufferComplete("multisample");

    makeTex(next.sceneTex, GL_RGBA16F, GL_RGBA, GL_FLOAT, w, h);
    makeTex(next.depthTex, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, w, h);
    glBindTexture(GL_TEXTURE_2D, next.depthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &next.resolveFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, next.resolveFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, next.sceneTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, next.depthTex, 0);
    ok = framebufferComplete("resolve") && ok;

    const int bw = std::max(1, w / 2), bh = std::max(1, h / 2);
    for (int i = 0; i < 2; ++i) {
        makeTex(next.bloomTex[i], GL_RGBA16F, GL_RGBA, GL_FLOAT, bw, bh);
        glGenFramebuffers(1, &next.bloomFbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, next.bloomFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, next.bloomTex[i], 0);
        ok = framebufferComplete(i == 0 ? "bloom A" : "bloom B") && ok;
    }
    makeTex(next.aoTex, GL_R8, GL_RED, GL_UNSIGNED_BYTE, bw, bh);
    glGenFramebuffers(1, &next.aoFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, next.aoFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, next.aoTex, 0);
    ok = framebufferComplete("SSAO") && ok;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    postOk_ = ok;
    if (ok) {
        targets_ = next;
    } else {
        destroyTargets(next);
        std::fprintf(stderr, "[renderer] post-processing disabled; rendering directly\n");
    }
}

void Renderer::renderPost() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    // resolve MSAA colour + depth into textures
    glBindFramebuffer(GL_READ_FRAMEBUFFER, targets_.msaaFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targets_.resolveFbo);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindVertexArray(postVao_);
    const int bw = std::max(1, width_ / 2), bh = std::max(1, height_ / 2);
    auto bindTex = [](int unit, GLuint tex) { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, tex); };

    // SSAO at half resolution from the resolved depth; skipped while the split screen is active because the
    // two views have different projections and this pass only knows the first one
    const bool ssaoOn = ssaoOk_ && targets_.aoFbo && debugMode_ != RenderDebugMode::NoSsao && !split_;
    if (ssaoOn) {
        glViewport(0, 0, bw, bh);
        glBindFramebuffer(GL_FRAMEBUFFER, targets_.aoFbo);
        ssao_.use();
        bindTex(0, targets_.depthTex); ssao_.set("uDepth", 0);
        ssao_.set("uProj", proj_);
        ssao_.set("uTanHalf", Vec2(1.0f / proj_.m[0][0], 1.0f / proj_.m[1][1]));
        ssao_.set("uNear", cam_.znear); ssao_.set("uFar", cam_.zfar);
        ssao_.set("uRadius", 0.7f); ssao_.set("uStrength", 0.8f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // bright pass -> half resolution, then two separable blur rounds (the second widens the glow)
    glViewport(0, 0, bw, bh);
    const bool bloomEnabled = debugMode_ != RenderDebugMode::NoBloom;
    if (bloomEnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, targets_.bloomFbo[0]);
        bright_.use();
        bindTex(0, targets_.sceneTex); bright_.set("uTex", 0); bright_.set("uThreshold", 0.72f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        blur_.use();
        blur_.set("uTex", 0);
        for (int round = 0; round < 2; ++round) {
            const float spread = round == 0 ? 1.0f : 2.2f;
            glBindFramebuffer(GL_FRAMEBUFFER, targets_.bloomFbo[1]);
            bindTex(0, targets_.bloomTex[0]);
            blur_.set("uDir", Vec2(spread / (float)bw, 0));
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindFramebuffer(GL_FRAMEBUFFER, targets_.bloomFbo[0]);
            bindTex(0, targets_.bloomTex[1]);
            blur_.set("uDir", Vec2(0, spread / (float)bh));
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    }

    // composite to the window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    composite_.use();
    bindTex(0, targets_.sceneTex); composite_.set("uScene", 0);
    if (bloomEnabled) {
        bindTex(1, targets_.bloomTex[0]);
        composite_.set("uBloomStrength", 0.55f);
    } else {
        composite_.set("uBloomStrength", 0.0f);
    }
    bindTex(2, targets_.depthTex); composite_.set("uDepth", 2);
    bindTex(3, targets_.aoTex); composite_.set("uAo", 3);
    composite_.set("uAoTexel", Vec2(1.0f / (float)bw, 1.0f / (float)bh));
    composite_.set("uAoAmount", ssaoOn ? 0.9f : 0.0f);
    composite_.set("uTexel", Vec2(1.0f / (float)width_, 1.0f / (float)height_));
    composite_.set("uNear", cam_.znear);
    composite_.set("uFar", cam_.zfar);
    composite_.set("uOutline", debugMode_ == RenderDebugMode::NoOutline ? 0.0f : outlineStrength_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderSecondView() {
    // 1) stencil-mark the second view's half-plane, resetting colour / depth there
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    mask_.use();
    mask_.set("uCenter", Vec2(width_ * 0.5f, height_ * 0.5f));
    mask_.set("uNormal", Vec2(splitN_.x, -splitN_.y));   // gl_FragCoord is bottom-up
    mask_.set("uClear", Vec3(clear_.r, clear_.g, clear_.b));
    glBindVertexArray(postVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);

    // 2) draw the same submissions from the second camera, only where stencil == 1.
    // Billboards were built for the first camera; both views share one orientation so they still face the viewer.
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    const Camera camA = cam_; const Mat4 viewA = view_, projA = proj_;
    cam_ = camB_; view_ = viewB_; proj_ = projB_;
    renderScenePass(false);
    renderUnlit(unlitAlpha_, Blend::Alpha);
    renderUnlit(unlitAdd_, Blend::Additive);
    cam_ = camA; view_ = viewA; proj_ = projA;
    glDisable(GL_STENCIL_TEST);
}

void Renderer::endFrame() {
    if (targetW_ >= 0 && (targetW_ != width_ || targetH_ != height_)) createTargets(width_, height_);
    const bool usePost = postOk_ && debugMode_ != RenderDebugMode::Direct;
    const bool useShadows = shadowOk_ && debugMode_ != RenderDebugMode::NoShadows;
    if (useShadows) renderShadowPass();
    glBindFramebuffer(GL_FRAMEBUFFER, usePost ? targets_.msaaFbo : 0);
    if (!useShadows) {
        lit_.use();
        lit_.set("uLightVP", Mat4::identity());
    }
    renderScenePass(true);
    renderUnlit(unlitAlpha_, Blend::Alpha);
    renderUnlit(unlitAdd_, Blend::Additive);
    if (split_) renderSecondView();
    if (usePost) renderPost();
    renderUi();
}

} // namespace dc
