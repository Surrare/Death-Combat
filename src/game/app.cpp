#include "game/app.h"
#include "core/png.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace dc {

namespace {
// Strict, clean, pleasant architectural UI palette (no garish neon pulsing)
const Color WHITE(0.94f, 0.95f, 0.97f), GOLD(0.85f, 0.72f, 0.42f), RED(0.78f, 0.24f, 0.26f),
            DIM(0.60f, 0.63f, 0.70f), CYAN(0.48f, 0.76f, 0.88f),
            PANEL_BG(0.055f, 0.060f, 0.075f, 0.94f), PANEL_HDR(0.085f, 0.092f, 0.115f, 0.96f),
            PANEL_BORDER(0.24f, 0.26f, 0.32f, 0.90f), ITEM_BG(0.090f, 0.098f, 0.122f, 0.85f),
            ITEM_SEL_BG(0.16f, 0.15f, 0.13f, 0.95f),
            UI_PANEL(0.035f, 0.04f, 0.055f, 0.92f), UI_PANEL_HI(0.11f, 0.13f, 0.18f, 0.95f),
            UI_BORDER(0.22f, 0.25f, 0.32f, 0.85f), UI_SUBTEXT(0.68f, 0.72f, 0.78f, 1.0f);
const char* MENU_ITEMS[] = {"HUNT MODE", "VERSUS CPU", "VERSUS 2P", "SETTINGS", "QUIT"};

// Title menu geometry, shared by update (mouse hit-testing) and render so the two never disagree.
struct TitleLayout {
    float panelX, panelY, panelW, panelH;
    float itemX, itemW, itemY0;   // highlight rectangle of item 0 (itemY0 is its top edge)
    static constexpr float ITEM_H = 42, ITEM_STEP = 50;
    float textY(int i) const { return itemY0 + 10 + i * ITEM_STEP; }
    float itemY(int i) const { return itemY0 + i * ITEM_STEP; }
};
TitleLayout titleLayout(int width, int height) {
    TitleLayout l;
    l.panelX = 36; l.panelY = 32;
    l.panelW = std::min(440.0f, width * 0.38f);
    l.panelH = height - 64.0f;
    l.itemX = l.panelX + 28; l.itemW = l.panelW - 56;
    l.itemY0 = l.panelY + 196;
    return l;
}
}

bool App::init(GLFWwindow* window, const AppOptions& opts) {
    window_ = window;
    opts_ = opts;
    if (!renderer_.init()) return false;
    if (!rig_.init()) return false;
    settings_.load();
    if (opts_.renderDebugModeSet)
        settings_.renderMode = static_cast<int>(opts_.renderDebugMode);
    if (opts_.devMode)
        settings_.devMode = true;
    if (opts_.showHelp)
        showHelp_ = true;
    renderer_.setDebugMode(static_cast<RenderDebugMode>(settings_.renderMode));
    input_.attach(window, &settings_);
    applyVideoSettings();
    audio_.init();
    fxRng_ = Rng(opts.seed ? opts.seed : (uint64_t)glfwGetTime() * 1000 + 7);
    for (int i = 0; i < ARENA_COUNT; ++i) arenas_[i].build(i);
    {
        MeshBuilder b;
        b.addPlane(Vec3(0, 0, 0), 40, 40, 20, Vec3(0.2f, 0.18f, 0.22f), Vec3(0.16f, 0.14f, 0.18f));
        titleFloor_.upload(b);
        b.clear();
        b.addSphere(Vec3(0, 0, 0), 0.5f, 8, 12, Vec3(1, 1, 1));
        projectileMesh_.upload(b);
    }
    const int ch = std::clamp(opts.character, 0, (int)roster().size() - 1);
    selCursor_[0] = ch;
    if (opts.demo == 1) { startFight(ch, (ch + 1) % (int)roster().size(), FightMode::Demo, settings_.aiDifficulty, opts.arena); }
    else if (opts.demo == 2) { startHunt(ch, std::max(0, opts.arena)); }
    else if (opts.demo == 4) { openSettings(Scene::Title); if (opts.arena >= 0) settingsTab_ = std::clamp(opts.arena, 0, 3); }
    else if (opts.demo == 3) { scene_ = Scene::Select; pendingMode_ = FightMode::Hunt; selectFor_ = 0; }
    else if (opts.demo == 5 || opts.demo == 7) {
        pendingMode_ = opts.demo == 7 ? FightMode::Hunt : FightMode::VsCpu; selChosen_[0] = ch; selChosen_[1] = (ch + 1) % (int)roster().size();
        scene_ = Scene::StageSelect; stageCursor_ = std::max(0, opts.arena);
    }
    else if (opts.demo == 6) { startFight(ch, (ch + 1) % (int)roster().size(), FightMode::Versus, settings_.aiDifficulty, opts.arena); }
    return true;
}

void App::takeScreenshot(const std::string& path) {
    std::vector<uint8_t> px((size_t)width_ * height_ * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    if (writePng(path.c_str(), width_, height_, px.data())) std::printf("screenshot saved: %s\n", path.c_str());
}

void App::addBanner(const std::string& t, Color c, float life, float scale, float yFrac) {
    Banner b; b.text = t; b.col = c; b.life = life; b.maxLife = life; b.scale = scale; b.yFrac = yFrac;
    banners_.push_back(b);
}
void App::addFloat(Vec3 pos, const std::string& t, Color c, float life, float scale) {
    FloatText f; f.pos = pos; f.text = t; f.col = c; f.life = life; f.maxLife = life; f.scale = scale;
    floats_.push_back(f);
}

void App::drawPanelText(float x, float y, const std::vector<std::string>& lines, float scale, Color col, float lineGap) {
    const float lineHeight = renderer_.textLineHeight(scale) * lineGap / 1.5f;
    for (size_t i = 0; i < lines.size(); ++i) renderer_.text(x, y + (float)i * lineHeight, lines[i], scale, col);
}

// --------------------------------------------------------------------- frame --
void App::frame(float dt, int w, int h) {
    width_ = w; height_ = h;
    renderer_.resize(w, h);
    input_.poll(dt);
    if (opts_.showcase) updateShowcase(dt);
    frameDt_ = dt;
    if (dt > 1e-4f) fpsSmoothed_ = lerp(fpsSmoothed_, 1.0f / dt, 0.08f);
    if (shaderReloadToast_ > 0.0f) shaderReloadToast_ = std::max(0.0f, shaderReloadToast_ - dt);
    updateDevKeys();
    renderer_.setDebugMode(static_cast<RenderDebugMode>(settings_.renderMode));
    renderer_.setLanguage(settings_.language);

    float effDt = animPaused_ ? 0.0f : dt;
    time_ += effDt;
    rig_.setTime(time_);

    audio_.setVolumes(settings_.master, settings_.music, settings_.sfx);
    if (menuSoundsActive()) {
        const PadState& m = input_.menu();
        bool capturing = scene_ == Scene::Settings && settingsCapturing_;
        if (!capturing) {
            if (m.justPressed(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) audio_.play(Sfx::UiMove);
            if (m.justPressed(BTN_START | BTN_LIGHT) || input_.leftClicked()) audio_.play(Sfx::UiConfirm);
            if (m.justPressed(BTN_BACK) || input_.rightClicked()) audio_.play(Sfx::UiBack);
        }
    }

    switch (scene_) {
    case Scene::Title: updateTitle(effDt); renderTitle(); break;
    case Scene::Select: updateSelect(effDt); renderSelect(); break;
    case Scene::StageSelect: updateStageSelect(effDt); renderStageSelect(); break;
    case Scene::Hunt: updateHunt(effDt); renderHunt(); break;
    case Scene::Fight: updateFight(effDt); renderFight(); break;
    case Scene::Settings: updateSettings(dt); renderSettings(); break;
    }

    switch (scene_) {
    case Scene::Hunt: audio_.setMusic(MusicMode::Hunt, hunt_.senseLevel()); break;
    case Scene::Fight: audio_.setMusic(MusicMode::Fight); break;
    case Scene::Settings: if (settingsReturn_ == Scene::Hunt || settingsReturn_ == Scene::Fight) audio_.setMusic(MusicMode::Off); else audio_.setMusic(MusicMode::Menu); break;
    default: audio_.setMusic(MusicMode::Menu); break;
    }

    // screenshots: F12, or automatic capture for the command line option
    bool f12 = glfwGetKey(window_, GLFW_KEY_F12) == GLFW_PRESS;
    if (f12 && !f12Was_) {
        std::filesystem::create_directories("screenshots");
        takeScreenshot("screenshots/shot_" + std::to_string(++shotCounter_) + ".png");
    }
    f12Was_ = f12;
    if (!opts_.screenshotPath.empty() && !shotTaken_ && time_ >= opts_.screenshotTime) {
        takeScreenshot(opts_.screenshotPath);
        shotTaken_ = true;
        quit_ = true;
    }
}

// Menus are on screen (as opposed to hunting / fighting), so navigation keys should click.
bool App::menuSoundsActive() const {
    switch (scene_) {
    case Scene::Title: case Scene::Select: case Scene::StageSelect: case Scene::Settings: return true;
    case Scene::Hunt: return huntPaused_ || huntComplete_;
    case Scene::Fight: return fightMode_ != FightMode::Demo && (paused_ || sim_.phase() == Phase::MatchOver);
    }
    return false;
}

// --------------------------------------------------------------------- title --
void App::updateTitle(float dt) {
    titleTime_ += dt;
    if (input_.mouseCaptured()) input_.setMouseCaptured(false);
    const PadState& p = input_.menu();
    int n = 5;
    if (p.justPressed(BTN_DOWN)) menuIndex_ = (menuIndex_ + 1) % n;
    if (p.justPressed(BTN_UP)) menuIndex_ = (menuIndex_ + n - 1) % n;
    bool clicked = false;
    const TitleLayout tl = titleLayout(width_, height_);
    for (int i = 0; i < n; ++i) {
        if (!input_.mouseIn(tl.itemX, tl.itemY(i), tl.itemW, TitleLayout::ITEM_H)) continue;
        if (input_.mouseMoved() || input_.leftClicked()) menuIndex_ = i;
        clicked = input_.leftClicked();
    }
    if (p.justPressed(BTN_START) || p.justPressed(BTN_LIGHT) || clicked) {
        switch (menuIndex_) {
        case 0: pendingMode_ = FightMode::Hunt; scene_ = Scene::Select; selectFor_ = 0; selChosen_[0] = selChosen_[1] = -1; break;
        case 1: pendingMode_ = FightMode::VsCpu; scene_ = Scene::Select; selectFor_ = 0; selChosen_[0] = selChosen_[1] = -1; break;
        case 2: pendingMode_ = FightMode::Versus; scene_ = Scene::Select; selectFor_ = 0; selChosen_[0] = selChosen_[1] = -1; break;
        case 3: openSettings(Scene::Title); break;
        case 4: quit_ = true; break;
        }
    }
}

void App::renderTitle() {
    Camera cam;
    cam.eye = Vec3(std::sin(titleTime_ * 0.18f) * 5.6f, 2.3f, std::cos(titleTime_ * 0.18f) * 5.6f);
    cam.target = Vec3(0, 1.1f, 0);
    cam.fovDeg = 44;
    applyOrbitCamera(cam);
    renderer_.beginFrame(cam, Color(0.025f, 0.028f, 0.036f));
    renderer_.setDirectionalLight(Vec3(-0.4f, -1.0f, -0.3f), Vec3(0.85f, 0.82f, 0.78f), 0.30f);
    renderer_.setShadowFocus(Vec3(0, 0, 0), 8.0f);
    renderer_.setFog(Vec3(0.025f, 0.028f, 0.036f), 0.0035f);
    renderer_.addPointLight(Vec3(-3, 3, 2), Vec3(0.85f, 0.65f, 0.45f), 10);
    renderer_.addPointLight(Vec3(3, 3, 2), Vec3(0.45f, 0.55f, 0.85f), 10);
    renderer_.draw(titleFloor_, Mat4::identity(), Color(1, 1, 1), 0, 1);
    const auto& ros = roster();
    // the roster stands in a ring, each holding their signature menu pose
    for (int i = 0; i < (int)ros.size(); ++i) {
        float a = (float)i / (float)ros.size() * 2 * PI;
        Vec3 pos(std::sin(a) * 2.65f, 0, std::cos(a) * 2.65f);
        float yaw = std::atan2(-pos.x, -pos.z) + PI;  // face outward
        Pose p = menuPose(i, time_ + i * 0.5f);
        rig_.draw(renderer_, p, pos, yaw, ros[i], 1.0f);
        if (ros[i].style == StyleType::Stand) {
            Vec3 inward = normalize(Vec3(-pos.x, 0, -pos.z));
            Vec3 sp = pos + inward * 0.82f + Vec3(0, 0.30f + 0.08f * std::sin(time_ * 2.0f + i), 0);
            rig_.draw(renderer_, standMenuPose(i, time_ + i * 0.5f), sp, yaw, ros[i], 1.05f, 0.42f, 0, true, false);
        }
    }
    // Strict, clean architectural navigation panel on the left
    const TitleLayout tl = titleLayout(width_, height_);
    const float panelX = tl.panelX, panelY = tl.panelY, panelW = tl.panelW, panelH = tl.panelH;
    renderer_.rect(panelX, panelY, panelW, panelH, PANEL_BG);
    renderer_.rect(panelX, panelY, panelW, 134, PANEL_HDR);
    renderer_.rectOutline(panelX, panelY, panelW, panelH, 1.5f, PANEL_BORDER);
    renderer_.rect(panelX, panelY, panelW, 3, GOLD);
    renderer_.rect(panelX + 28, panelY + 134, panelW - 56, 1, PANEL_BORDER);

    float cx = panelX + panelW * 0.5f;
    renderer_.text(panelX + 28, panelY + 26, "DEATH COMBAT", 4.6f, WHITE);
    renderer_.text(panelX + 28, panelY + 78, "3D TACTICAL STAND & MODE FIGHTER", 1.42f, GOLD);
    renderer_.text(panelX + 28, panelY + 98, "Ali Yerzhanuly, Daulet Ulgaisyn, Alikhan Kokey", 1.25f, WHITE);
    renderer_.text(panelX + 28, panelY + 115, "Astana IT University  |  Group: IT-2502", 1.20f, CYAN);

    const char* subLabels[5] = {
        "Stalk 5 rivals through 3D labyrinths & duel",
        "Single match against configurable CPU AI",
        "Local 2-player duel (shared or split view)",
        "Gameplay, Audio/Video, P1 & P2 Controls",
        "Exit to desktop"
    };
    renderer_.text(panelX + 28, panelY + 158, "SELECT MODE", 1.35f, DIM);
    for (int i = 0; i < 5; ++i) {
        bool sel = i == menuIndex_;
        float iy = tl.itemY(i);
        renderer_.rect(tl.itemX, iy, tl.itemW, TitleLayout::ITEM_H, sel ? ITEM_SEL_BG : ITEM_BG);
        renderer_.rectOutline(tl.itemX, iy, tl.itemW, TitleLayout::ITEM_H, 1.0f, sel ? GOLD : PANEL_BORDER);
        if (sel) renderer_.rect(tl.itemX, iy, 4, TitleLayout::ITEM_H, GOLD);
        renderer_.text(tl.itemX + 18, iy + 5, MENU_ITEMS[i], 2.0f, sel ? GOLD : WHITE);
        renderer_.text(tl.itemX + 18, iy + 26, subLabels[i], 1.15f, DIM);
    }

    // Quick reference card inside bottom of the left panel
    float qy = panelY + panelH - 132;
    renderer_.rect(tl.itemX, qy, tl.itemW, 84, ITEM_BG);
    renderer_.rectOutline(tl.itemX, qy, tl.itemW, 84, 1.0f, PANEL_BORDER);
    renderer_.text(tl.itemX + 14, qy + 10, "CONTROLS OVERVIEW", 1.3f, GOLD);
    renderer_.text(tl.itemX + 14, qy + 30, "P1: WASD MOVE   U/SHIFT 8-WAY DASH   J/K/L ATTACK   I STYLE", 1.18f, WHITE);
    renderer_.text(tl.itemX + 14, qy + 48, "P2: ARROWS MOVE   NUM 5/0 DASH   NUM 1/2/3 OR N/M/, ATTACK", 1.18f, DIM);
    renderer_.text(tl.itemX + 14, qy + 66, "8-WAY DASH: DIRECTION + DASH (OR DOUBLE-TAP ANY DIRECTION)", 1.12f, CYAN);

    renderer_.textCentered(cx, panelY + panelH - 28, "UP / DOWN  NAVIGATE      ENTER / CLICK  CONFIRM", 1.35f, DIM);
    drawDevOverlay();
    renderer_.endFrame();
}

// -------------------------------------------------------------------- select --
void App::updateSelect(float dt) {
    selectSpin_ += dt;
    const auto& ros = roster();
    int n = (int)ros.size();
    int who = selectFor_;
    const PadState& m = input_.menu();
    // left/right (and up/down) cycle through the roster
    if (m.justPressed(BTN_RIGHT) || m.justPressed(BTN_DOWN)) selCursor_[who] = (selCursor_[who] + 1) % n;
    if (m.justPressed(BTN_LEFT) || m.justPressed(BTN_UP)) selCursor_[who] = (selCursor_[who] + n - 1) % n;
    bool clicked = false;
    {
        float cardW = 170, cardH = 84, gap = 12, totalW = n * cardW + (n - 1) * gap, x0 = (width_ - totalW) * 0.5f;
        for (int i = 0; i < n; ++i) {
            if (!input_.mouseIn(x0 + i * (cardW + gap), height_ - 120, cardW, cardH)) continue;
            if (input_.mouseMoved() || input_.leftClicked()) selCursor_[who] = i;
            clicked = input_.leftClicked();
        }
    }
    if (m.justPressed(BTN_BACK) || input_.rightClicked()) {
        if (who == 1) { selectFor_ = 0; return; }   // step back to player 1's pick
        scene_ = Scene::Title; return;
    }
    if (m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT) || clicked) {
        selChosen_[who] = selCursor_[who];
        auto toStages = [&] { scene_ = Scene::StageSelect; stageCursor_ = 0; stageSpin_ = 0; stagePreviewGround_ = -1; };
        if (pendingMode_ == FightMode::Hunt) { toStages(); return; }
        if (pendingMode_ == FightMode::VsCpu) {
            int cpu = fxRng_.range(0, n - 2);
            if (cpu >= selChosen_[0]) cpu++;
            selChosen_[1] = cpu;
            toStages();
            return;
        }
        if (who == 0) { selectFor_ = 1; selCursor_[1] = (selCursor_[0] + 1) % n; }
        else toStages();
    }
}

void App::drawCharacterCard(int id, float x, float y, float w, float h, bool highlighted, bool defeated) {
    const Character& c = roster()[id];
    renderer_.rect(x, y, w, h, highlighted ? UI_PANEL_HI : UI_PANEL);
    renderer_.rect(x, y, w, 3, c.styleColor);
    renderer_.rectOutline(x, y, w, h, highlighted ? 1.5f : 1.0f, highlighted ? GOLD : UI_BORDER);
    if (highlighted) {
        renderer_.rect(x, y + 3, 3, h - 3, GOLD);
    }
    renderer_.text(x + 12, y + 14, c.name, 2.2f, defeated ? DIM : (highlighted ? GOLD : WHITE));
    renderer_.text(x + 12, y + 38, c.styleName, 1.35f, defeated ? DIM : c.styleColor);
    renderer_.text(x + 12, y + 58, c.traitName, 1.25f, defeated ? DIM : UI_SUBTEXT);
    if (defeated) renderer_.textRight(x + w - 10, y + 14, "DEFEATED", 1.3f, RED);
}

void App::renderSelect() {
    const auto& ros = roster();
    int n = (int)ros.size();
    int who = selectFor_;
    int cur = selCursor_[who];
    const Character& c = ros[cur];
    Camera cam;
    cam.eye = Vec3(0, 1.48f, 4.15f); cam.target = Vec3(0, 1.02f, 0); cam.fovDeg = 38;
    applyOrbitCamera(cam);
    renderer_.beginFrame(cam, Color(0.025f, 0.028f, 0.038f));
    renderer_.setDirectionalLight(Vec3(-0.3f, -1.0f, -0.5f), Vec3(0.92f, 0.88f, 0.82f), 0.34f);
    renderer_.setShadowFocus(Vec3(0, 0, 0), 5.0f);
    renderer_.setFog(c.styleColor.rgb() * 0.04f, 0.008f);
    renderer_.addPointLight(Vec3(-2, 2.5f, 2), c.styleColor.rgb() * 1.35f, 8);
    renderer_.addPointLight(Vec3(2, 1.5f, 2), Vec3(0.6f, 0.64f, 0.72f), 8);
    renderer_.draw(titleFloor_, Mat4::identity(), Color(0.65f, 0.67f, 0.75f), 0, 1);
    {
        const Color sc = c.styleColor;
        renderer_.addPointLight(Vec3(0, 3.2f, 3.0f), Vec3(1.0f, 0.95f, 0.88f) * 0.85f, 12);
        renderer_.addPointLight(Vec3(2.2f, 1.8f, -2.2f), sc.rgb() * 1.2f, 9);
        renderer_.draw(rig_.unitCylinder(), Mat4::translate(Vec3(0, 0.03f, 0)) * Mat4::scale(Vec3(2.5f, 0.06f, 2.5f)), Color(0.06f, 0.065f, 0.085f), 0, 0, false);
        renderer_.draw(rig_.unitCylinder(), Mat4::translate(Vec3(0, 0.065f, 0)) * Mat4::scale(Vec3(2.22f, 0.02f, 2.22f)), sc * 0.85f, 0.7f, 0, false);
        renderer_.draw(rig_.unitCylinder(), Mat4::translate(Vec3(0, 0.08f, 0)) * Mat4::scale(Vec3(2.08f, 0.03f, 2.08f)), Color(0.045f, 0.05f, 0.068f), 0, 0, false);
        renderer_.drawBillboard(Vec3(0, 1.45f, -2.6f), 7.5f, sc.withAlpha(0.14f), Blend::Additive, time_ * 0.06f);
    }
    float yaw = selectSpin_ * 0.55f;
    Pose p = menuPose(cur, time_);
    rig_.draw(renderer_, p, Vec3(0, 0, 0), yaw, c, 1.0f);
    if (c.style == StyleType::Stand) {
        Vec3 back(-std::sin(yaw), 0, -std::cos(yaw));
        Vec3 sp = back * 0.9f + Vec3(0.0f, 0.35f + 0.08f * std::sin(time_ * 1.8f), 0);
        rig_.draw(renderer_, standMenuPose(cur, time_), sp, yaw, c, 1.1f, 0.52f, 0, true, false);
        renderer_.drawBillboard(sp + Vec3(0, 1.2f, 0), 2.2f, c.styleColor.withAlpha(0.15f), Blend::Additive, time_ * 0.5f);
    } else {
        renderer_.drawBillboard(Vec3(0, 1.0f, 0), 2.4f, c.styleColor.withAlpha(0.11f), Blend::Additive, time_ * 0.5f);
    }

    std::string title = pendingMode_ == FightMode::Hunt ? "CHOOSE YOUR HUNTER" : (who == 0 ? "PLAYER 1  SELECT" : "PLAYER 2  SELECT");
    renderer_.textCentered(width_ * 0.5f, 26, title, 3.2f, WHITE);
    renderer_.rect(width_ * 0.5f - 140, 62, 280, 1, UI_BORDER);

    float cardW = 170, cardH = 84, gap = 12;
    float totalW = n * cardW + (n - 1) * gap;
    float x0 = (width_ - totalW) * 0.5f;
    for (int i = 0; i < n; ++i) drawCharacterCard(i, x0 + i * (cardW + gap), height_ - 120, cardW, cardH, i == cur, false);

    // Left dossier panel (character identity + unique trait + lore)
    float px = 42, py = 122;
    renderer_.rect(24, 86, 405, 430, UI_PANEL);
    renderer_.rectOutline(24, 86, 405, 430, 1, UI_BORDER);
    renderer_.rect(24, 86, 405, 2, c.styleColor);
    renderer_.text(px, 98, pendingMode_ == FightMode::Hunt ? "HUNTER DOSSIER" : "FIGHTER DOSSIER", 1.35f, GOLD);
    renderer_.text(px, py, c.name, 4.4f, WHITE);
    renderer_.text(px, py + 44, c.title, 1.8f, c.styleColor);
    renderer_.text(px, py + 72, (c.style == StyleType::Stand ? "STYLE: STAND  |  " : "STYLE: MODE  |  ") + c.styleName, 1.55f, UI_SUBTEXT);
    renderer_.text(px, py + 94, "VITALITY: " + std::to_string((int)c.hp) + " HP", 1.55f, DIM);

    // Unique Trait Box
    float ty = py + 122;
    renderer_.rect(px - 6, ty, 375, 76, Color(0.06f, 0.07f, 0.095f, 0.92f));
    renderer_.rectOutline(px - 6, ty, 375, 76, 1, UI_BORDER);
    renderer_.rect(px - 6, ty, 3, 76, GOLD);
    renderer_.text(px + 6, ty + 8, "UNIQUE TRAIT: " + c.traitName, 1.45f, GOLD);
    {
        std::vector<std::string> tlines; std::string tcur;
        const std::string tdesc = localizeText(c.traitDesc, settings_.language);
        std::istringstream twords(tdesc);
        for (std::string w; twords >> w;) {
            std::string cand = tcur.empty() ? w : tcur + " " + w;
            if (!tcur.empty() && renderer_.textWidth(cand, 1.35f) > 352.0f) {
                tlines.push_back(tcur);
                tcur = w;
            } else tcur = cand;
        }
        if (!tcur.empty()) tlines.push_back(tcur);
        drawPanelText(px + 6, ty + 30, tlines, 1.35f, WHITE, 1.35f);
    }

    // Lore, wrapped
    std::vector<std::string> lines; std::string cur_line;
    const std::string lore = localizeText(c.lore, settings_.language);
    std::istringstream words(lore);
    for (std::string word; words >> word;) {
        std::string candidate = cur_line.empty() ? word : cur_line + " " + word;
        if (!cur_line.empty() && renderer_.textWidth(candidate, 1.45f) > 368.0f) {
            lines.push_back(cur_line);
            cur_line = word;
        } else {
            cur_line = candidate;
        }
    }
    if (!cur_line.empty()) lines.push_back(cur_line);
    drawPanelText(px, ty + 90, lines, 1.45f, DIM, 1.38f);

    // Right command panel
    renderer_.rect(width_ - 444, 86, 420, 390, UI_PANEL);
    renderer_.rectOutline(width_ - 444, 86, 420, 390, 1, UI_BORDER);
    renderer_.rect(width_ - 444, 86, 420, 2, GOLD);
    renderer_.text(width_ - 426, 98, "COMMAND OVERVIEW", 1.35f, GOLD);
    std::vector<std::string> ml;
    ml.push_back("SPECIAL TECHNIQUES");
    for (const Move& m : c.moves) {
        if (m.kind == MoveKind::Special && m.buttons == BTN_LIGHT) {
            std::string mo = m.motion == Motion::QCF ? "F + LIGHT" : m.motion == Motion::QCB ? "B + LIGHT" : "F + HEAVY";
            ml.push_back("  " + m.name.substr(0, m.name.size() - 2) + "   [" + mo + "]");
        }
    }
    ml.push_back("");
    ml.push_back("SUPER ARTS");
    for (const Move& m : c.moves) if (m.kind == MoveKind::HHA) ml.push_back("  HHA (1 BAR): " + m.name + "  [F + L+M]");
    for (const Move& m : c.moves) if (m.kind == MoveKind::GHA) ml.push_back("  GHA (2 BAR): " + m.name + "  [F + L+M+H]");
    ml.push_back("");
    ml.push_back("UNIVERSAL MOBILITY");
    ml.push_back("  8-WAY DASH:  DIRECTION + DASH (OR 2X TAP)");
    ml.push_back("  SIDESTEP:    UP / DOWN");
    drawPanelText(width_ - 426, 126, ml, 1.45f, WHITE, 1.45f);

    renderer_.textCentered(width_ * 0.5f, height_ - 148, "ARROWS / MOUSE  SELECT     ENTER / CLICK  CONFIRM     ESC  BACK", 1.45f, DIM);
    drawDevOverlay();
    renderer_.endFrame();
}

// -------------------------------------------------------------- stage select --
namespace {
// Stage list geometry, shared by update (mouse hit-testing) and render so the two never disagree.
struct StageLayout {
    float panelX, panelY, panelW, panelH, itemX, itemW, itemY0;
    static constexpr float ITEM_H = 42, ITEM_STEP = 48;
    float itemY(int i) const { return itemY0 + i * ITEM_STEP; }
};
StageLayout stageLayout(int width, int height) {
    StageLayout l;
    l.panelX = 28; l.panelY = 86; l.panelW = std::min(450.0f, width * 0.39f); l.panelH = height - 140.0f;
    l.itemX = l.panelX + 18; l.itemW = l.panelW - 36; l.itemY0 = l.panelY + 46;
    return l;
}
}

void App::updateStageSelect(float dt) {
    stageSpin_ += dt;
    const bool forHunt = pendingMode_ == FightMode::Hunt;
    const int count = forHunt ? MAZE_THEME_COUNT : ARENA_COUNT, n = count + 1;   // last entry: random
    const PadState& m = input_.menu();
    if (m.justPressed(BTN_DOWN)) stageCursor_ = (stageCursor_ + 1) % n;
    if (m.justPressed(BTN_UP)) stageCursor_ = (stageCursor_ + n - 1) % n;
    bool clicked = false;
    const StageLayout L = stageLayout(width_, height_);
    for (int i = 0; i < n; ++i) {
        if (!input_.mouseIn(L.itemX, L.itemY(i), L.itemW, StageLayout::ITEM_H)) continue;
        if (input_.mouseMoved() || input_.leftClicked()) stageCursor_ = i;
        clicked = input_.leftClicked();
    }
    if (m.justPressed(BTN_BACK) || input_.rightClicked()) { scene_ = Scene::Select; selectFor_ = 0; return; }
    if (m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT) || clicked) {
        const int pick = stageCursor_ < count ? stageCursor_ : fxRng_.range(0, count - 1);
        if (forHunt) startHunt(selChosen_[0], pick);
        else startFight(selChosen_[0], selChosen_[1], pendingMode_, settings_.aiDifficulty, pick);
    }
}

void App::renderStageSelect() {
    const bool forHunt = pendingMode_ == FightMode::Hunt;
    const int count = forHunt ? MAZE_THEME_COUNT : ARENA_COUNT, n = count + 1;
    const bool random = stageCursor_ >= count;
    const int shown = random ? (int)(stageSpin_ / 1.6f) % count : stageCursor_;   // "random" flicks through them

    // ---- live preview behind the menu
    if (forHunt) {
        if (stagePreviewGround_ != shown) {
            std::vector<int> others;
            for (int i = 0; i < (int)roster().size(); ++i) if (i != selChosen_[0]) others.push_back(i);
            hunt_.init(selChosen_[0], others, 4242, shown);
            stagePreviewGround_ = shown;
        }
        const MazeTheme& T = mazeTheme(shown);
        const Vec3 c = hunt_.mazeCentre();
        const float span = T.size * Maze::CELL, a = stageSpin_ * 0.12f;
        Camera cam;
        cam.target = c + Vec3(span * 0.12f, 0, 0);
        cam.eye = c + Vec3(std::sin(a) * span * 0.5f, span * 0.6f, std::cos(a) * span * 0.5f);
        cam.fovDeg = 48; cam.zfar = 300;
        applyOrbitCamera(cam);
        renderer_.beginFrame(cam, T.sky);
        hunt_.render(renderer_, rig_, time_);
        renderer_.setFog(T.fog, T.fogDensity * 0.06f);
        renderer_.setDirectionalLight(T.sunDir, T.sunColor * 1.8f, std::min(0.6f, T.ambient * 1.9f));
        renderer_.setShadowFocus(c, span * 0.6f);
    } else {
        const Arena& ar = arenas_[shown];
        const float a = stageSpin_ * 0.16f;
        Camera cam;
        cam.target = Vec3(0, 1.3f, 0);
        cam.eye = Vec3(std::sin(a) * 12.5f, 4.2f, std::cos(a) * 12.5f);
        cam.fovDeg = 50; cam.zfar = 300;
        applyOrbitCamera(cam);
        renderer_.beginFrame(cam, ar.info().clear);
        renderer_.setShadowFocus(Vec3(), 12);
        const Character& c0 = roster()[selChosen_[0]];
        const Character& c1 = roster()[selChosen_[1]];
        ar.render(renderer_, rig_.unitBox(), time_, c0.styleColor, c1.styleColor);
        renderer_.addPointLight(cam.eye + Vec3(0, 1.5f, 0), Vec3(1.0f, 0.93f, 0.82f) * 0.8f, 18);
        const float gap = ar.info().shape.spawnGap * 0.5f;
        for (int i = 0; i < 2; ++i) {
            int cid = i ? selChosen_[1] : selChosen_[0];
            rig_.draw(renderer_, menuPose(cid, time_ + i * 0.4f), Vec3(i ? gap : -gap, 0, 0), i ? -PI * 0.5f : PI * 0.5f, i ? c1 : c0,
                      1.0f, 1.0f, 0, false, true, 0.01f);
        }
    }

    // ---- menu
    const StageLayout L = stageLayout(width_, height_);
    renderer_.textCentered(width_ * 0.5f, 26, forHunt ? "CHOOSE YOUR HUNTING GROUND" : "CHOOSE THE ARENA", 3.2f, WHITE);
    renderer_.rect(L.panelX, L.panelY, L.panelW, L.panelH, UI_PANEL);
    renderer_.rectOutline(L.panelX, L.panelY, L.panelW, L.panelH, 1, UI_BORDER);
    renderer_.rect(L.panelX, L.panelY, L.panelW, 2, GOLD);
    renderer_.text(L.panelX + 18, L.panelY + 14, forHunt ? "HUNTING GROUNDS" : "COMBAT ARENAS", 1.35f, GOLD);
    for (int i = 0; i < n; ++i) {
        bool sel = i == stageCursor_;
        const char* name = i < count ? (forHunt ? mazeTheme(i).name : arenaInfo(i).name) : "RANDOM";
        float y = L.itemY(i);
        renderer_.rect(L.itemX, y, L.itemW, StageLayout::ITEM_H, sel ? UI_PANEL_HI : Color(0.05f, 0.055f, 0.075f, 0.72f));
        renderer_.rectOutline(L.itemX, y, L.itemW, StageLayout::ITEM_H, 1, sel ? GOLD : UI_BORDER);
        if (sel) {
            renderer_.rect(L.itemX, y, 3, StageLayout::ITEM_H, GOLD);
        }
        renderer_.text(L.itemX + 16, y + 11, name, 2.1f, sel ? GOLD : WHITE);
    }
    // description + arena hazard/trait, wrapped to the panel
    const std::string blurb = localizeText(random ? "LET FATE DECIDE." : (forHunt ? mazeTheme(shown).blurb : arenaInfo(shown).blurb), settings_.language);
    std::vector<std::string> lines; std::string line;
    std::istringstream words(blurb);
    for (std::string word; words >> word;) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && renderer_.textWidth(candidate, 1.5f) > L.panelW - 44) { lines.push_back(line); line = word; }
        else line = candidate;
    }
    if (!line.empty()) lines.push_back(line);
    if (random) lines.insert(lines.begin(), localizeText(forHunt ? mazeTheme(shown).name : arenaInfo(shown).name, settings_.language));
    float descY = L.itemY(n) + 14;
    drawPanelText(L.panelX + 22, descY, lines, 1.5f, DIM, 1.38f);

    if (!forHunt && arenaInfo(shown).trait) {
        float traitY = descY + lines.size() * 22.0f + 14.0f;
        std::vector<std::string> tlines; std::string tline;
        const std::string ttext = localizeText(arenaInfo(shown).trait, settings_.language);
        std::istringstream twords(ttext);
        for (std::string w; twords >> w;) {
            std::string cand = tline.empty() ? w : tline + " " + w;
            if (!tline.empty() && renderer_.textWidth(cand, 1.32f) > L.itemW - 24.0f) {
                tlines.push_back(tline);
                tline = w;
            } else tline = cand;
        }
        if (!tline.empty()) tlines.push_back(tline);
        float boxH = 34.0f + tlines.size() * 19.0f;
        renderer_.rect(L.itemX, traitY, L.itemW, boxH, Color(0.06f, 0.07f, 0.095f, 0.92f));
        renderer_.rectOutline(L.itemX, traitY, L.itemW, boxH, 1, UI_BORDER);
        renderer_.rect(L.itemX, traitY, 3, boxH, shown == 2 ? RED : GOLD);
        renderer_.text(L.itemX + 12, traitY + 8, "STAGE MECHANIC", 1.3f, shown == 2 ? RED : GOLD);
        drawPanelText(L.itemX + 12, traitY + 28, tlines, 1.32f, WHITE, 1.35f);
    }

    if (!forHunt) {
        const std::string vs = roster()[selChosen_[0]].name + "  VS  " + roster()[selChosen_[1]].name;
        renderer_.textRight(width_ - 32, height_ - 82, vs, 2.8f, WHITE);
    } else {
        renderer_.textRight(width_ - 32, height_ - 82, roster()[selChosen_[0]].name, 2.8f, roster()[selChosen_[0]].styleColor);
    }
    renderer_.textCentered(width_ * 0.5f, height_ - 34, "ARROWS / MOUSE  SELECT     ENTER / CLICK  CONFIRM     ESC  BACK", 1.45f, DIM);
    drawDevOverlay();
    renderer_.endFrame();
}

// ---------------------------------------------------------------------- hunt --
void App::startHunt(int playerChar, int ground) {
    huntPlayerChar_ = playerChar;
    huntOrder_.clear();
    const auto& ros = roster();
    for (int i = 0; i < (int)ros.size(); ++i) if (i != playerChar) huntOrder_.push_back(i);
    uint64_t seed = opts_.seed ? opts_.seed : (uint64_t)(glfwGetTime() * 1000.0) + 17;
    hunt_.init(playerChar, huntOrder_, seed, ground);
    huntEngaged_ = -1; huntEngageTimer_ = 0; huntBannerTimer_ = 6.0f; huntPaused_ = false; huntComplete_ = false;
    huntDefeatedCount_ = 0; huntCompleteTimer_ = 0;
    scene_ = Scene::Hunt;
    input_.setMouseCaptured(true);
}

void App::updateHunt(float dt) {
    const PadState& pad = input_.menu();
    if (huntComplete_) {
        huntCompleteTimer_ += dt;
        if (input_.mouseCaptured()) input_.setMouseCaptured(false);
        if (huntCompleteTimer_ > 1.5f && (pad.justPressed(BTN_START) || pad.justPressed(BTN_LIGHT) || pad.justPressed(BTN_BACK) || input_.leftClicked())) scene_ = Scene::Title;
        hunt_.update(dt, PadState(), false);
        return;
    }
    if (pad.justPressed(BTN_BACK) && huntEngaged_ < 0) {
        huntPaused_ = !huntPaused_;
        huntPauseIndex_ = 0;
        input_.setMouseCaptured(!huntPaused_);
    }
    if (huntPaused_) {
        if (pad.justPressed(BTN_DOWN)) huntPauseIndex_ = (huntPauseIndex_ + 1) % 3;
        if (pad.justPressed(BTN_UP)) huntPauseIndex_ = (huntPauseIndex_ + 2) % 3;
        bool clicked = false;
        for (int i = 0; i < 3; ++i) {
            if (!input_.mouseIn(width_ * 0.5f - 180, height_ * 0.5f + i * 44 - 6, 360, 36)) continue;
            if (input_.mouseMoved() || input_.leftClicked()) huntPauseIndex_ = i;
            clicked = input_.leftClicked();
        }
        if (pad.justPressed(BTN_START) || pad.justPressed(BTN_LIGHT) || clicked) {
            if (huntPauseIndex_ == 0) { huntPaused_ = false; input_.setMouseCaptured(true); }
            else if (huntPauseIndex_ == 1) openSettings(Scene::Hunt);
            else { scene_ = Scene::Title; input_.setMouseCaptured(false); }
        }
        return;
    }
    if (!input_.mouseCaptured() && glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) input_.setMouseCaptured(true);
    if (huntBannerTimer_ > 0) huntBannerTimer_ -= dt;

    if (huntEngaged_ >= 0) {
        huntEngageTimer_ += dt;
        hunt_.update(dt * 0.15f, PadState(), false);
        if (huntEngageTimer_ > 2.2f) {
            int difficulty = huntDefeatedCount_ < 2 ? 0 : (huntDefeatedCount_ < 4 ? 1 : 2);
            const int opp = hunt_.opponents()[huntEngaged_].charId;
            startFight(huntPlayerChar_, opp, FightMode::Hunt, difficulty, homeArena(opp));   // they fight you on home ground
        }
        return;
    }
    int engaged = hunt_.update(dt, pad, true);
    if (engaged >= 0) {
        huntEngaged_ = engaged; huntEngageTimer_ = 0;
        audio_.play(Sfx::Engage);
        input_.setMouseCaptured(false);
    }
}

void App::renderHunt() {
    Camera cam = hunt_.camera();
    cam.fovDeg = (float)settings_.fovDeg;
    if (huntEngaged_ >= 0) {
        // dramatic zoom on the found opponent
        const HuntOpponent& o = hunt_.opponents()[huntEngaged_];
        float t = smoothstep(0, 1, huntEngageTimer_ / 1.2f);
        Vec3 toP = normalize(hunt_.playerPos() - o.pos);
        Vec3 side = cross(toP, Vec3(0, 1, 0));
        Vec3 eye = o.pos + toP * 2.4f + side * 1.1f + Vec3(0, 1.4f, 0);
        cam.eye = lerp(cam.eye, eye, t);
        cam.target = lerp(cam.target, o.pos + Vec3(0, 1.3f, 0), t);
        cam.fovDeg = lerp((float)settings_.fovDeg, 40.0f, t);
    }
    applyOrbitCamera(cam);
    renderer_.beginFrame(cam, hunt_.maze().theme().sky);
    hunt_.render(renderer_, rig_, hunt_.worldTime());

    const auto& ros = roster();
    float cx = width_ * 0.5f;
    // --- HUD: Death Sense
    float level = hunt_.senseLevel();
    float pulse = hunt_.heartPulse();
    float hx = 40, hy = 38;
    renderer_.rect(24, 24, 330, 84, UI_PANEL);
    renderer_.rectOutline(24, 24, 330, 84, 1, UI_BORDER);
    renderer_.rect(24, 24, 3, 84, GOLD);
    renderer_.text(hx, hy, "DEATH SENSE", 1.8f, GOLD);
    renderer_.rect(hx, hy + 24, 260, 12, Color(0.08f, 0.09f, 0.12f, 0.9f));
    renderer_.rect(hx, hy + 24, 260 * level, 12, mix(Color(0.32f, 0.48f, 0.85f), Color(0.92f, 0.22f, 0.25f), level));
    float hs = 18 + 12 * pulse * (0.4f + level);
    renderer_.rect(hx + 288 - hs * 0.5f, hy + 30 - hs * 0.5f, hs, hs, Color(0.92f, 0.25f, 0.28f, 0.5f + 0.45f * pulse));
    if (level > 0.22f && hunt_.senseDirection() >= 0) {
        const float dirYaw[4] = {0.0f, PI * 0.5f, PI, -PI * 0.5f};
        float rel = wrapAngle(dirYaw[hunt_.senseDirection()] - hunt_.cameraYaw());
        const char* label = std::fabs(rel) < 0.4f ? "AHEAD" : (std::fabs(rel) > 2.7f ? "BEHIND" : (rel > 0 ? "RIGHT" : "LEFT"));
        renderer_.text(hx, hy + 46, std::string("TRAIL: ") + label, 1.6f, UI_SUBTEXT);
    }
    if (hunt_.anyOpponentHunting()) renderer_.textCentered(cx, 56, "YOU ARE BEING HUNTED", 2.1f, RED);

    // roster status
    float rx = width_ - 220, ry = 40;
    renderer_.rect(rx - 12, ry - 16, 200, 150, UI_PANEL);
    renderer_.rectOutline(rx - 12, ry - 16, 200, 150, 1, UI_BORDER);
    renderer_.text(rx, ry, "TARGETS", 1.8f, GOLD);
    for (size_t i = 0; i < hunt_.opponents().size(); ++i) {
        const HuntOpponent& o = hunt_.opponents()[i];
        const Character& c = ros[o.charId];
        float y = ry + 26 + (float)i * 22;
        renderer_.rect(rx, y + 2, 10, 10, o.defeated ? Color(0.28f, 0.3f, 0.34f) : c.styleColor);
        renderer_.text(rx + 18, y, c.name, 1.65f, o.defeated ? DIM : WHITE);
        if (o.defeated) renderer_.text(rx + 110, y, "DOWN", 1.45f, RED);
    }
    // player
    renderer_.text(40, height_ - 50, ros[huntPlayerChar_].name, 2.3f, ros[huntPlayerChar_].styleColor);
    if (settings_.showHudPrompts) {
        renderer_.text(40, height_ - 28, "WASD / ARROWS MOVE   MOUSE LOOK   SHIFT RUN   ENTER ENGAGE   ESC PAUSE", 1.45f, DIM);
    }

    if (huntBannerTimer_ > 0) {
        float a = clampf(huntBannerTimer_, 0, 1);
        renderer_.textCentered(cx, height_ * 0.22f, "THE HUNT BEGINS", 4.6f, GOLD.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.22f + 48, std::to_string(hunt_.remaining()) + " OPPONENTS HIDE IN THESE HALLS. FIND THEM.", 1.9f, WHITE.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.22f + 74, "FOLLOW YOUR DEATH SENSE. WALK INTO THEM TO FIGHT.", 1.8f, DIM.withAlpha(a));
    }
    if (hunt_.engageCandidate() >= 0 && huntEngaged_ < 0) {
        const Character& c = ros[hunt_.opponents()[hunt_.engageCandidate()].charId];
        renderer_.textCentered(cx, height_ * 0.68f, "ENGAGE " + c.name + "   [ENTER / ATTACK]", 2.2f, GOLD);
    } else if (hunt_.nearestVisible() >= 0 && huntEngaged_ < 0) {
        const Character& c = ros[hunt_.opponents()[hunt_.nearestVisible()].charId];
        renderer_.textCentered(cx, height_ * 0.68f, c.name + " IS NEAR", 1.9f, c.styleColor);
    }
    if (huntEngaged_ >= 0) {
        const Character& c = ros[hunt_.opponents()[huntEngaged_].charId];
        float t = huntEngageTimer_;
        float a = clampf(t * 2.0f, 0, 1);
        renderer_.rect(0, height_ * 0.3f, width_, 120, Color(0.02f, 0.025f, 0.035f, 0.82f * a));
        renderer_.textCentered(cx, height_ * 0.3f + 18, "OPPONENT FOUND", 2.8f, GOLD.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.3f + 52, c.name + "  -  " + c.title, 3.6f, WHITE.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.3f + 92, c.styleName, 1.9f, c.styleColor.withAlpha(a));
    }
    if (huntPaused_) {
        renderer_.rect(0, 0, width_, height_, Color(0.01f, 0.012f, 0.018f, 0.78f));
        renderer_.rect(cx - 220, height_ * 0.32f, 440, 290, UI_PANEL);
        renderer_.rectOutline(cx - 220, height_ * 0.32f, 440, 290, 1, UI_BORDER);
        renderer_.rect(cx - 220, height_ * 0.32f, 440, 3, GOLD);
        renderer_.textCentered(cx, height_ * 0.35f, "PAUSED", 3.8f, WHITE);
        const char* items[3] = {"RESUME", "SETTINGS", "QUIT TO TITLE"};
        for (int i = 0; i < 3; ++i) {
            float y = height_ * 0.5f + i * 44;
            bool sel = (i == huntPauseIndex_);
            renderer_.rect(cx - 180, y - 6, 360, 36, sel ? UI_PANEL_HI : Color(0.05f, 0.055f, 0.075f, 0.75f));
            renderer_.rectOutline(cx - 180, y - 6, 360, 36, 1, sel ? GOLD : UI_BORDER);
            if (sel) renderer_.rect(cx - 180, y - 6, 3, 36, GOLD);
            renderer_.textCentered(cx, y + 3, items[i], 2.2f, sel ? GOLD : WHITE);
        }
        renderer_.textCentered(cx, height_ * 0.85f, "UP / DOWN / MOUSE  CHOOSE     ENTER / CLICK  CONFIRM     ESC  RESUME", 1.45f, DIM);
    }
    if (huntComplete_) {
        float a = clampf(huntCompleteTimer_, 0, 1);
        renderer_.rect(0, 0, width_, height_, Color(0.01f, 0.012f, 0.018f, 0.78f * a));
        renderer_.textCentered(cx, height_ * 0.35f, "HUNT COMPLETE", 5.8f, GOLD.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.35f + 76, ros[huntPlayerChar_].name + " STANDS ALONE IN THE HALLS", 2.3f, WHITE.withAlpha(a));
        if (huntCompleteTimer_ > 1.5f) renderer_.textCentered(cx, height_ * 0.7f, "PRESS ENTER OR CLICK", 1.9f, DIM);
    }
    drawDevOverlay();
    renderer_.endFrame();
}

} // namespace dc
