// Forward renderer: directional light with shadow map, point lights, fog, unlit 3D batches
// (particles / glow), and a 2D HUD batch with smooth TrueType text.
#pragma once
#include <glad/gl.h>
#include <vector>
#include <string>
#include "core/math.h"
#include "core/font.h"
#include "core/localization.h"
#include "gfx/shader.h"
#include "gfx/mesh.h"

namespace dc {

struct PointLight {
    Vec3 position;
    Vec3 color;
    float radius = 10.0f;
};

struct DrawCmd {
    const Mesh* mesh;
    Mat4 model;
    Color color;
    float emissive;   // 0 = lit only, 1 = fully self-lit
    int pattern;      // 0 none, 1 floor tiles, 2 bricks, 3 metal panels
    bool castShadow;
};

struct Camera {
    Vec3 eye{0, 2, 6};
    Vec3 target{0, 1, 0};
    float fovDeg = 50.0f;
    float znear = 0.1f, zfar = 200.0f;
};

enum class Blend { Alpha, Additive };
enum class RenderDebugMode { Full, Direct, NoBloom, NoOutline, NoShadows };

class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init();
    void resize(int width, int height) { width_ = width > 0 ? width : 1; height_ = height > 0 ? height : 1; }
    void setLanguage(Language language) { language_ = language; }
    void setDebugMode(RenderDebugMode mode) { debugMode_ = mode; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Scene setup for this frame
    void beginFrame(const Camera& cam, const Color& clear);
    void setDirectionalLight(const Vec3& dir, const Vec3& color, float ambient);
    void setShadowFocus(const Vec3& center, float extent) { shadowCenter_ = center; shadowExtent_ = extent; }
    void setFog(const Vec3& color, float density) { fogColor_ = color; fogDensity_ = density; }
    void setRimColor(const Vec3& c) { rimColor_ = c; }   // edge light on characters / props
    void setOutline(float strength) { outlineStrength_ = strength; }   // 0 = off; screen-space ink outline
    void clearPointLights() { pointLights_.clear(); }
    void addPointLight(const Vec3& pos, const Vec3& color, float radius) { pointLights_.push_back({pos, color, radius}); }

    // Split screen: a second camera with the same orientation renders the pixels on the far side of a line
    // through the screen centre, perpendicular to splitNormal (screen pixels, y down). Reset by beginFrame.
    void setSecondView(const Camera& cam, const Vec2& splitNormal);
    bool hasSecondView() const { return split_; }
    // World point to screen pixels, choosing whichever view shows that point. False if behind / not visible.
    bool projectToScreen(const Vec3& p, float& sx, float& sy) const;

    // 3D submissions
    void draw(const Mesh& mesh, const Mat4& model, const Color& color = Color(), float emissive = 0.0f, int pattern = 0, bool castShadow = true);
    void drawUnlitTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Color& col, Blend blend);
    void drawBillboard(const Vec3& center, float size, const Color& col, Blend blend, float rotation = 0.0f);
    void drawLine3D(const Vec3& a, const Vec3& b, float thickness, const Color& col, Blend blend);

    // 2D submissions (pixel coordinates, origin top-left)
    void rect(float x, float y, float w, float h, const Color& col);
    void rectOutline(float x, float y, float w, float h, float t, const Color& col);
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color& col);
    void line(float x0, float y0, float x1, float y1, float thickness, const Color& col);
    void text(float x, float y, const std::string& s, float scale, const Color& col);
    void textCentered(float cx, float y, const std::string& s, float scale, const Color& col);
    void textRight(float rx, float y, const std::string& s, float scale, const Color& col);
    float textWidth(const std::string& s, float scale) const;
    float textLineHeight(float scale) const;

    // Renders everything submitted since beginFrame.
    void endFrame();

    const Camera& camera() const { return cam_; }
    Mat4 viewMatrix() const { return view_; }
    Mat4 projMatrix() const { return proj_; }
    Vec3 cameraRight() const { return camRight_; }
    Vec3 cameraUp() const { return camUp_; }

private:
    struct UnlitVertex { Vec3 pos; Color col; float u, v; };
    struct UiVertex { float x, y, u, v; Color col; };
    struct PostTargets {
        GLuint msaaFbo = 0, msaaColor = 0, msaaDepth = 0;
        GLuint resolveFbo = 0, sceneTex = 0, depthTex = 0;
        GLuint bloomFbo[2] = {0, 0}, bloomTex[2] = {0, 0};
    };

    bool buildFontAtlas();
    void renderShadowPass();
    void renderScenePass(bool clear);
    void renderSecondView();
    void renderUnlit(std::vector<UnlitVertex>& verts, Blend blend);
    void renderUi();
    // post-processing: scene is rendered to an HDR MSAA target, then bloom + outlines + grading are applied
    void createTargets(int w, int h);
    void destroyTargets();
    void destroyTargets(PostTargets& targets);
    void renderPost();

    int width_ = 1280, height_ = 720;
    Camera cam_;
    Mat4 view_, proj_;
    Vec3 camRight_{1, 0, 0}, camUp_{0, 1, 0};
    Color clear_;
    Vec3 sunDir_{-0.4f, -1.0f, -0.3f};
    Vec3 sunColor_{1, 1, 1};
    float ambient_ = 0.3f;
    Vec3 shadowCenter_;
    float shadowExtent_ = 12.0f;
    Vec3 fogColor_{0, 0, 0};
    float fogDensity_ = 0.0f;
    std::vector<PointLight> pointLights_;
    std::vector<DrawCmd> opaque_, transparent_;
    std::vector<UnlitVertex> unlitAlpha_, unlitAdd_;
    std::vector<UiVertex> ui_;

    Shader lit_, depth_, unlit_, ui2d_, bright_, blur_, composite_, mask_;
    bool split_ = false;
    Camera camB_;
    Mat4 viewB_, projB_;
    Vec2 splitN_{1, 0};
    GLuint postVao_ = 0;
    int targetW_ = 0, targetH_ = 0;
    bool postOk_ = false;
    float outlineStrength_ = 0.7f;
    Vec3 rimColor_{0.45f, 0.6f, 1.0f};
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    bool shadowOk_ = false;
    int shadowSize_ = 2048;
    GLuint unlitVao_ = 0, unlitVbo_ = 0;
    GLuint uiVao_ = 0, uiVbo_ = 0;
    GLuint fontTex_ = 0;
    FontAtlas font_;
    Language language_ = Language::English;
    RenderDebugMode debugMode_ = RenderDebugMode::Full;
    PostTargets targets_;
};

} // namespace dc
