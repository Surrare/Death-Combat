#include "gfx/renderer.h"
#include "core/font.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace dc {
namespace {
constexpr float TEXT_CAP_HEIGHT = 9.0f;
FontFace textFace(float scale) { return scale >= 2.4f ? FontFace::Display : FontFace::Body; }
}

// ---------------------------------------------------------------- shaders ----
static const char* LIT_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uModel, uView, uProj, uLightVP;
out vec3 vWorld; out vec3 vNormal; out vec3 vColor; out vec4 vShadow;
void main(){
    vec4 w = uModel * vec4(aPos,1.0);
    vWorld = w.xyz;
    vNormal = normalize(transpose(inverse(mat3(uModel))) * aNormal);
    vColor = aColor;
    vShadow = uLightVP * w;
    gl_Position = uProj * uView * w;
})";

static const char* LIT_FS = R"(#version 330 core
in vec3 vWorld; in vec3 vNormal; in vec3 vColor; in vec4 vShadow;
out vec4 FragColor;
uniform vec4 uColor; uniform float uEmissive; uniform int uPattern;
uniform vec3 uSunDir, uSunColor; uniform float uAmbient;
uniform vec3 uEye; uniform vec3 uFogColor; uniform float uFogDensity; uniform vec3 uRimColor;
uniform int uNumLights; uniform vec4 uLightPos[16]; uniform vec3 uLightColor[16];
uniform sampler2DShadow uShadowMap;
uniform int uShadowEnabled;

// 0 on a grid line, 1 inside a cell; lines fade out with distance so they do not shimmer when the camera moves
float gridLine(vec2 f, vec2 width, vec2 fw){
    vec2 e = smoothstep(width, width + fw * 1.5, f);
    float line = e.x * e.y;
    return mix(line, 0.94, clamp(max(fw.x, fw.y) * 6.0, 0.0, 1.0));
}

float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453); }

float shadowFactor(vec3 n){
    if (uShadowEnabled == 0) return 1.0;
    vec3 p = vShadow.xyz / vShadow.w * 0.5 + 0.5;
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0) return 1.0;
    float bias = max(0.0025 * (1.0 - dot(n, -uSunDir)), 0.0008);
    float s = 0.0;
    vec2 texel = vec2(1.0/2048.0);
    for (int x=-1;x<=1;x++) for (int y=-1;y<=1;y++)
        s += texture(uShadowMap, vec3(p.xy + vec2(x,y)*texel, p.z - bias));
    return s / 9.0;
}

vec3 patternColor(vec3 base){
    if (uPattern == 1) { // floor tiles
        vec2 c = floor(vWorld.xz * 0.5);
        float h = hash(c);
        vec2 f = fract(vWorld.xz * 0.5);
        float edge = gridLine(f, vec2(0.03), fwidth(vWorld.xz * 0.5));
        return base * (0.85 + 0.3*h) * (0.55 + 0.45*edge);
    } else if (uPattern == 2) { // bricks on walls
        float row = floor(vWorld.y * 3.0);
        float along = (abs(vNormal.x) > 0.5 ? vWorld.z : vWorld.x) * 1.5 + mod(row, 2.0) * 0.5;
        vec2 c = vec2(floor(along), row);
        float h = hash(c);
        vec2 f = vec2(fract(along), fract(vWorld.y * 3.0));
        float mortar = gridLine(f, vec2(0.05, 0.08), vec2(fwidth(along), fwidth(vWorld.y * 3.0)));
        return base * (0.8 + 0.4*h) * (0.5 + 0.5*mortar);
    } else if (uPattern == 3) { // metal panels
        vec2 uv = (abs(vNormal.y) > 0.5) ? vWorld.xz : (abs(vNormal.x) > 0.5 ? vWorld.zy : vWorld.xy);
        vec2 c = floor(uv * 0.75);
        vec2 f = fract(uv * 0.75);
        float groove = gridLine(f, vec2(0.04), fwidth(uv * 0.75));
        return base * (0.9 + 0.2*hash(c)) * (0.6 + 0.4*groove);
    }
    return base;
}

void main(){
    vec3 n = normalize(vNormal);
    vec3 base = patternColor(vColor * uColor.rgb);
    vec3 v = normalize(uEye - vWorld);
    // hemisphere ambient
    float hemi = 0.5 + 0.5 * n.y;
    vec3 col = base * uAmbient * mix(0.6, 1.2, hemi);
    // sun
    vec3 l = normalize(-uSunDir);
    float ndl0 = dot(n, l);
    float ndl = mix(max(ndl0, 0.0), pow(ndl0 * 0.5 + 0.5, 2.0), 0.35);   // slight wrap keeps shadow sides readable
    float sh = shadowFactor(n);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), 32.0) * 0.25;
    float specMul = uPattern == 1 ? 0.25 : 1.0;   // floor: tame glints that crawl as the camera moves
    col += (base * ndl + vec3(spec * specMul)) * uSunColor * sh;
    // point lights
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
    // rim
    // rim: characters and props get a cool edge light so dark outfits do not vanish into the arena
    float rim = pow(1.0 - max(dot(n, v), 0.0), 2.6);
    float rimMul = uPattern == 0 ? 1.0 : 0.15;
    col += rim * rimMul * (base * 0.35 + uRimColor * 0.22) * (0.35 + 0.65 * smoothstep(-0.3, 0.6, -dot(n, l) * 0.0 + n.y * 0.4 + 0.4));
    col = mix(col, uColor.rgb * vColor, uEmissive);
    // fog
    float dist = length(uEye - vWorld);
    float fog = 1.0 - exp(-uFogDensity * dist * dist);
    col = mix(col, uFogColor, clamp(fog, 0.0, 1.0));
    FragColor = vec4(col, uColor.a);
})";

static const char* DEPTH_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uModel, uLightVP;
void main(){ gl_Position = uLightVP * uModel * vec4(aPos,1.0); })";
static const char* DEPTH_FS = R"(#version 330 core
void main(){})";

static const char* UNLIT_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in vec2 aUV;
uniform mat4 uViewProj;
out vec4 vColor; out vec2 vUV;
void main(){ vColor = aColor; vUV = aUV; gl_Position = uViewProj * vec4(aPos,1.0); })";
static const char* UNLIT_FS = R"(#version 330 core
in vec4 vColor; in vec2 vUV; out vec4 FragColor;
void main(){
    float d = length(vUV);
    float soft = 1.0 - smoothstep(0.15, 1.0, d);
    soft = soft * soft;
    FragColor = vec4(vColor.rgb, vColor.a * soft);
})";

static const char* UI_VS = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform vec2 uScreen;
out vec2 vUV; out vec4 vColor;
void main(){
    vUV = aUV; vColor = aColor;
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
})";
static const char* UI_FS = R"(#version 330 core
in vec2 vUV; in vec4 vColor; out vec4 FragColor;
uniform sampler2D uFont;
void main(){
    float distance = texture(uFont, vUV).r;
    float edge = 128.0 / 255.0;
    float aa = clamp(0.5 * fwidth(distance), 1.0 / 255.0, 0.25);
    float a = vUV.x < -0.5 ? 1.0 : smoothstep(edge - aa, edge + aa, distance);
    FragColor = vec4(vColor.rgb, vColor.a * a);
})";


static const char* POST_VS = R"(#version 330 core
out vec2 vUV;
void main(){
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";

static const char* BRIGHT_FS = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex; uniform float uThreshold;
void main(){
    vec3 c = texture(uTex, vUV).rgb;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - uThreshold, 0.0) / max(l, 1e-4);
    FragColor = vec4(c * k, 1.0);
})";

static const char* BLUR_FS = R"(#version 330 core
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
})";

// Split screen mask: marks the second view's half-plane with stencil = 1, fills it with the clear colour at far depth.
static const char* MASK_FS = R"(#version 330 core
out vec4 FragColor;
uniform vec2 uCenter, uNormal; uniform vec3 uClear;
void main(){
    if (dot(gl_FragCoord.xy - uCenter, uNormal) <= 0.0) discard;
    FragColor = vec4(uClear, 1.0);
    gl_FragDepth = 1.0;
})";

static const char* COMPOSITE_FS = R"(#version 330 core
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
    col += texture(uBloom, vUV).rgb * uBloomStrength;

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
})";

// ------------------------------------------------------------------- init ----
bool Renderer::init() {
    if (!lit_.compile(LIT_VS, LIT_FS, "lit")) return false;
    if (!depth_.compile(DEPTH_VS, DEPTH_FS, "depth")) return false;
    if (!unlit_.compile(UNLIT_VS, UNLIT_FS, "unlit")) return false;
    if (!ui2d_.compile(UI_VS, UI_FS, "ui")) return false;
    // post-processing is optional: if any part fails the renderer falls back to drawing straight to the window
    bool postShaders = bright_.compile(POST_VS, BRIGHT_FS, "bright") && blur_.compile(POST_VS, BLUR_FS, "blur") &&
                       composite_.compile(POST_VS, COMPOSITE_FS, "composite");
    glGenVertexArrays(1, &postVao_);
    if (!mask_.compile(POST_VS, MASK_FS, "split mask")) return false;
    if (!postShaders) targetW_ = targetH_ = -1;   // never create targets

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
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
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
    sunDir_ = normalize(dir); sunColor_ = color; ambient_ = ambient;
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
    lit_.set("uShadowEnabled", 1);
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

    auto submit = [&](const DrawCmd& c) {
        lit_.set("uModel", c.model);
        lit_.set("uColor", c.color);
        lit_.set("uEmissive", c.emissive);
        lit_.set("uPattern", c.pattern);
        c.mesh->draw();
    };
    for (const DrawCmd& c : opaque_) submit(c);

    if (!transparent_.empty()) {
        // sort back to front
        std::sort(transparent_.begin(), transparent_.end(), [&](const DrawCmd& a, const DrawCmd& b) {
            Vec3 pa = a.model.transformPoint(Vec3(0, 0, 0)), pb = b.model.transformPoint(Vec3(0, 0, 0));
            return (pa - cam_.eye).lengthSq() > (pb - cam_.eye).lengthSq();
        });
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (const DrawCmd& c : transparent_) submit(c);
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

void Renderer::destroyTargets() {
    if (msaaFbo_) glDeleteFramebuffers(1, &msaaFbo_);
    if (resolveFbo_) glDeleteFramebuffers(1, &resolveFbo_);
    if (msaaColor_) glDeleteRenderbuffers(1, &msaaColor_);
    if (msaaDepth_) glDeleteRenderbuffers(1, &msaaDepth_);
    if (sceneTex_) glDeleteTextures(1, &sceneTex_);
    if (depthTex_) glDeleteTextures(1, &depthTex_);
    for (int i = 0; i < 2; ++i) {
        if (bloomFbo_[i]) glDeleteFramebuffers(1, &bloomFbo_[i]);
        if (bloomTex_[i]) glDeleteTextures(1, &bloomTex_[i]);
        bloomFbo_[i] = bloomTex_[i] = 0;
    }
    msaaFbo_ = resolveFbo_ = msaaColor_ = msaaDepth_ = sceneTex_ = depthTex_ = 0;
    postOk_ = false;
}

void Renderer::createTargets(int w, int h) {
    destroyTargets();
    targetW_ = w; targetH_ = h;
    GLint maxSamples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const int samples = std::max(1, std::min(4, (int)maxSamples));

    glGenFramebuffers(1, &msaaFbo_);
    glGenRenderbuffers(1, &msaaColor_);
    glGenRenderbuffers(1, &msaaDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, msaaDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaaColor_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msaaDepth_);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    auto makeTex = [&](GLuint& tex, GLenum internal, GLenum format, GLenum type, int tw, int th) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, internal, tw, th, 0, format, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    makeTex(sceneTex_, GL_RGBA16F, GL_RGBA, GL_FLOAT, w, h);
    makeTex(depthTex_, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, w, h);   // stencil drives the split screen
    glBindTexture(GL_TEXTURE_2D, depthTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &resolveFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex_, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);
    ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    const int bw = std::max(1, w / 2), bh = std::max(1, h / 2);
    for (int i = 0; i < 2; ++i) {
        makeTex(bloomTex_[i], GL_RGBA16F, GL_RGBA, GL_FLOAT, bw, bh);
        glGenFramebuffers(1, &bloomFbo_[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex_[i], 0);
        ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    postOk_ = ok;
    if (!ok) {
        std::fprintf(stderr, "[renderer] post-processing targets unavailable, drawing directly\n");
        destroyTargets();
        targetW_ = w; targetH_ = h;   // do not retry every frame
    }
}

void Renderer::renderPost() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    // resolve MSAA colour + depth into textures
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
    glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindVertexArray(postVao_);
    const int bw = std::max(1, width_ / 2), bh = std::max(1, height_ / 2);
    auto bindTex = [](int unit, GLuint tex) { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(GL_TEXTURE_2D, tex); };

    // bright pass -> half resolution, then two separable blur rounds (the second widens the glow)
    glViewport(0, 0, bw, bh);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[0]);
    bright_.use();
    bindTex(0, sceneTex_); bright_.set("uTex", 0); bright_.set("uThreshold", 0.72f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    blur_.use();
    blur_.set("uTex", 0);
    for (int round = 0; round < 2; ++round) {
        const float spread = round == 0 ? 1.0f : 2.2f;
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[1]);
        bindTex(0, bloomTex_[0]);
        blur_.set("uDir", Vec2(spread / (float)bw, 0));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[0]);
        bindTex(0, bloomTex_[1]);
        blur_.set("uDir", Vec2(0, spread / (float)bh));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // composite to the window
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    composite_.use();
    bindTex(0, sceneTex_); composite_.set("uScene", 0);
    bindTex(1, bloomTex_[0]); composite_.set("uBloom", 1);
    bindTex(2, depthTex_); composite_.set("uDepth", 2);
    composite_.set("uTexel", Vec2(1.0f / (float)width_, 1.0f / (float)height_));
    composite_.set("uNear", cam_.znear);
    composite_.set("uFar", cam_.zfar);
    composite_.set("uBloomStrength", 0.55f);
    composite_.set("uOutline", outlineStrength_);
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
    renderShadowPass();
    if (postOk_) glBindFramebuffer(GL_FRAMEBUFFER, msaaFbo_);
    renderScenePass(true);
    renderUnlit(unlitAlpha_, Blend::Alpha);
    renderUnlit(unlitAdd_, Blend::Additive);
    if (split_) renderSecondView();
    if (postOk_) renderPost();
    renderUi();
}

} // namespace dc
