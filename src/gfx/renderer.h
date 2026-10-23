// Forward renderer: directional light with shadow map, point lights, fog, unlit 3D batches
// (particles / glow), and a 2D HUD batch with bitmap text.
#pragma once
#include <glad/gl.h>
#include <vector>
#include <string>
#include "core/math.h"
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

class Renderer {
public:
    bool init();
    void resize(int width, int height) { width_ = width; height_ = height; }
    int width() const { return width_; }
    int height() const { return height_; }

    // Scene setup for this frame
    void beginFrame(const Camera& cam, const Color& clear);
    void setDirectionalLight(const Vec3& dir, const Vec3& color, float ambient);
    void setShadowFocus(const Vec3& center, float extent) { shadowCenter_ = center; shadowExtent_ = extent; }
    void setFog(const Vec3& color, float density) { fogColor_ = color; fogDensity_ = density; }
    void clearPointLights() { pointLights_.clear(); }
    void addPointLight(const Vec3& pos, const Vec3& color, float radius) { pointLights_.push_back({pos, color, radius}); }

    // 3D submissions
    void draw(const Mesh& mesh, const Mat4& model, const Color& color = Color(), float emissive = 0.0f, int pattern = 0, bool castShadow = true);
    void drawUnlitTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Color& col, Blend blend);
    void drawBillboard(const Vec3& center, float size, const Color& col, Blend blend, float rotation = 0.0f);
    void drawLine3D(const Vec3& a, const Vec3& b, float thickness, const Color& col, Blend blend);

    // 2D submissions (pixel coordinates, origin top-left)
    void rect(float x, float y, float w, float h, const Color& col);
    void rectOutline(float x, float y, float w, float h, float t, const Color& col);
    void text(float x, float y, const std::string& s, float scale, const Color& col);
    void textCentered(float cx, float y, const std::string& s, float scale, const Color& col);
    void textRight(float rx, float y, const std::string& s, float scale, const Color& col);
    float textWidth(const std::string& s, float scale) const { return (float)s.size() * 6.0f * scale; }

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

    void buildFontAtlas();
    void renderShadowPass();
    void renderScenePass();
    void renderUnlit(std::vector<UnlitVertex>& verts, Blend blend);
    void renderUi();

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

    Shader lit_, depth_, unlit_, ui2d_;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    int shadowSize_ = 2048;
    GLuint unlitVao_ = 0, unlitVbo_ = 0;
    GLuint uiVao_ = 0, uiVbo_ = 0;
    GLuint fontTex_ = 0;
    int fontCols_ = 0;
    int glyphIndex_[128];
};

} // namespace dc
