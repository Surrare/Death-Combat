#include "gfx/renderer.h"
#include "core/font.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace dc {

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
    vNormal = normalize(mat3(uModel) * aNormal);
    vColor = aColor;
    vShadow = uLightVP * w;
    gl_Position = uProj * uView * w;
})";

static const char* LIT_FS = R"(#version 330 core
in vec3 vWorld; in vec3 vNormal; in vec3 vColor; in vec4 vShadow;
out vec4 FragColor;
uniform vec4 uColor; uniform float uEmissive; uniform int uPattern;
uniform vec3 uSunDir, uSunColor; uniform float uAmbient;
uniform vec3 uEye; uniform vec3 uFogColor; uniform float uFogDensity;
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
    float ndl = max(dot(n, l), 0.0);
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
        float pdl = max(dot(n, ld), 0.0);
        vec3 ph = normalize(ld + v);
        float ps = pow(max(dot(n, ph), 0.0), 24.0) * 0.3;
        col += (base * pdl + vec3(ps * specMul)) * uLightColor[i] * att;
    }
    // rim
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.15;
    col += rim * base;
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
    float a = vUV.x < -0.5 ? 1.0 : texture(uFont, vUV).r;
    FragColor = vec4(vColor.rgb, vColor.a * a);
})";

// ------------------------------------------------------------------- init ----
bool Renderer::init() {
    if (!lit_.compile(LIT_VS, LIT_FS, "lit")) return false;
    if (!depth_.compile(DEPTH_VS, DEPTH_FS, "depth")) return false;
    if (!unlit_.compile(UNLIT_VS, UNLIT_FS, "unlit")) return false;
    if (!ui2d_.compile(UI_VS, UI_FS, "ui")) return false;

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

    buildFontAtlas();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    return true;
}

void Renderer::buildFontAtlas() {
    int count = 0;
    const Glyph* glyphs = fontGlyphs(count);
    fontCols_ = count;
    const int cw = 8, ch = 8;   // 8 px cells, glyphs are 5x7: 3 px of padding avoids bleeding
    std::vector<unsigned char> pixels((size_t)(count * cw * ch), 0);
    for (int i = 0; i < 128; ++i) glyphIndex_[i] = 0;
    for (int g = 0; g < count; ++g) {
        glyphIndex_[(int)glyphs[g].ch] = g;
        for (int r = 0; r < 7; ++r)
            for (int c = 0; c < 5; ++c)
                if (glyphs[g].rows[r][c] == '#')
                    pixels[(size_t)(r * count * cw + g * cw + c)] = 255;
    }
    for (int c = 'a'; c <= 'z'; ++c) glyphIndex_[c] = glyphIndex_[c - 'a' + 'A'];
    glGenTextures(1, &fontTex_);
    glBindTexture(GL_TEXTURE_2D, fontTex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, count * cw, ch, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

void Renderer::text(float x, float y, const std::string& s, float scale, const Color& col) {
    float cw = 6.0f * scale, ch = 8.0f * scale;
    float gu = 1.0f / (float)fontCols_;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 128) c = '?';
        int g = glyphIndex_[c];
        float u0 = g * gu, u1 = (g + 0.75f) * gu;   // 6 of the 8 cell pixels
        float px = x + (float)i * cw;
        UiVertex v0{px, y, u0, 0, col}, v1{px + cw, y, u1, 0, col}, v2{px + cw, y + ch, u1, 1, col}, v3{px, y + ch, u0, 1, col};
        ui_.push_back(v0); ui_.push_back(v1); ui_.push_back(v2);
        ui_.push_back(v0); ui_.push_back(v2); ui_.push_back(v3);
    }
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

void Renderer::renderScenePass() {
    glViewport(0, 0, width_, height_);
    glClearColor(clear_.r, clear_.g, clear_.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

void Renderer::endFrame() {
    renderShadowPass();
    renderScenePass();
    renderUnlit(unlitAlpha_, Blend::Alpha);
    renderUnlit(unlitAdd_, Blend::Additive);
    renderUi();
}

} // namespace dc
