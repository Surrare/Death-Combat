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
const Color WHITE(1, 1, 1), GOLD(1.0f, 0.82f, 0.3f), RED(0.95f, 0.2f, 0.2f), DIM(0.55f, 0.55f, 0.6f), CYAN(0.4f, 0.9f, 1.0f);
const char* MENU_ITEMS[] = {"HUNT MODE", "VERSUS CPU", "VERSUS 2P", "SETTINGS", "QUIT"};

// Title menu geometry, shared by update (mouse hit-testing) and render so the two never disagree.
struct TitleLayout {
    float panelX, panelY, panelW, panelH;
    float itemX, itemW, itemY0;   // highlight rectangle of item 0 (itemY0 is its top edge)
    static constexpr float ITEM_H = 38, ITEM_STEP = 46;
    float textY(int i) const { return itemY0 + 8 + i * ITEM_STEP; }
    float itemY(int i) const { return itemY0 + i * ITEM_STEP; }
};
TitleLayout titleLayout(int width, int height) {
    TitleLayout l;
    l.panelX = 28; l.panelY = 24;
    l.panelW = std::min(460.0f, width * 0.42f);
    l.panelH = height - 48.0f;
    l.itemX = l.panelX + 34; l.itemW = l.panelW - 56;
    l.itemY0 = l.panelY + 202 - 8;
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
    if (opts.demo == 1) { startFight(ch, (ch + 1) % (int)roster().size(), FightMode::Demo, 2, opts.arena); }
    else if (opts.demo == 2) { startHunt(ch, std::max(0, opts.arena)); }
    else if (opts.demo == 4) { openSettings(Scene::Title); }
    else if (opts.demo == 3) { scene_ = Scene::Select; pendingMode_ = FightMode::Hunt; selectFor_ = 0; }
    else if (opts.demo == 5 || opts.demo == 7) {
        pendingMode_ = opts.demo == 7 ? FightMode::Hunt : FightMode::VsCpu; selChosen_[0] = ch; selChosen_[1] = (ch + 1) % (int)roster().size();
        scene_ = Scene::StageSelect; stageCursor_ = std::max(0, opts.arena);
    }
    else if (opts.demo == 6) { startFight(ch, (ch + 1) % (int)roster().size(), FightMode::Versus, 1, opts.arena); }
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
    renderer_.setDebugMode(static_cast<RenderDebugMode>(settings_.renderMode));
    renderer_.setLanguage(settings_.language);
    input_.poll(dt);
    time_ += dt;
    frameDt_ = dt;
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
    case Scene::Title: updateTitle(dt); renderTitle(); break;
    case Scene::Select: updateSelect(dt); renderSelect(); break;
    case Scene::StageSelect: updateStageSelect(dt); renderStageSelect(); break;
    case Scene::Hunt: updateHunt(dt); renderHunt(); break;
    case Scene::Fight: updateFight(dt); renderFight(); break;
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
    cam.eye = Vec3(std::sin(titleTime_ * 0.25f) * 5.5f, 2.4f, std::cos(titleTime_ * 0.25f) * 5.5f);
    cam.target = Vec3(0, 1.1f, 0);
    cam.fovDeg = 45;
    renderer_.beginFrame(cam, Color(0.02f, 0.02f, 0.03f));
    renderer_.setDirectionalLight(Vec3(-0.4f, -1.0f, -0.3f), Vec3(0.8f, 0.75f, 0.7f), 0.25f);
    renderer_.setShadowFocus(Vec3(0, 0, 0), 8.0f);
    renderer_.setFog(Vec3(0.02f, 0.02f, 0.03f), 0.004f);
    renderer_.addPointLight(Vec3(-3, 3, 2), Vec3(1.0f, 0.2f, 0.25f) * 1.2f, 10);
    renderer_.addPointLight(Vec3(3, 3, 2), Vec3(0.5f, 0.3f, 1.0f) * 1.2f, 10);
    renderer_.draw(titleFloor_, Mat4::identity(), Color(1, 1, 1), 0, 1);
    const auto& ros = roster();
    // the roster stands in a ring
    for (int i = 0; i < (int)ros.size(); ++i) {
        float a = (float)i / (float)ros.size() * 2 * PI;
        Vec3 pos(std::sin(a) * 2.6f, 0, std::cos(a) * 2.6f);
        float yaw = std::atan2(-pos.x, -pos.z) + PI;  // face outward
        Pose p = idlePose(time_ + i);
        p.spineYaw = 0; p.headYaw = 0;
        rig_.draw(renderer_, p, pos, yaw, ros[i], 1.0f);
        if (ros[i].style == StyleType::Stand) {
            Vec3 inward = normalize(Vec3(-pos.x, 0, -pos.z));
            Vec3 sp = pos + inward * 0.8f + Vec3(0, 0.3f + 0.1f * std::sin(time_ * 2 + i), 0);
            rig_.draw(renderer_, standIdlePose(time_ + i), sp, yaw, ros[i], 1.05f, 0.4f, 0, true, false);
        }
    }
    // A restrained crimson dossier frames the mode select while keeping the arena visible.
    const TitleLayout tl = titleLayout(width_, height_);
    const float panelX = tl.panelX, panelY = tl.panelY, panelW = tl.panelW, panelH = tl.panelH;
    renderer_.rect(panelX, panelY, panelW, panelH, Color(0.015f, 0.012f, 0.02f, 0.86f));
    renderer_.rectOutline(panelX, panelY, panelW, panelH, 2, Color(0.7f, 0.12f, 0.16f, 0.9f));
    renderer_.rect(panelX + 16, panelY + 16, 3, panelH - 32, Color(0.95f, 0.16f, 0.2f, 0.85f));
    renderer_.rect(panelX + 30, panelY + 184, panelW - 60, 1, Color(1.0f, 0.82f, 0.3f, 0.35f));
    float cx = panelX + panelW * 0.5f + 8;
    float pulse = 0.85f + 0.15f * std::sin(titleTime_ * 3.0f);
    renderer_.textCentered(cx + 2, 48 + 2, "DEATH COMBAT", 5.5f, Color(0, 0, 0, 0.7f));
    renderer_.textCentered(cx, 48, "DEATH COMBAT", 5.5f, Color(0.95f, 0.15f, 0.2f) * pulse);
    renderer_.textCentered(cx, 108, "SEEK YOUR OPPONENT.  STAND AND FIGHT.", 1.65f, DIM);
    renderer_.textCentered(cx, 146, "F / B + LIGHT  SPECIAL     F + HEAVY  RISING", 1.35f, GOLD);
    renderer_.textCentered(cx, 166, "REPEAT LIGHT  EASY BEAT", 1.35f, DIM);
    for (int i = 0; i < 5; ++i) {
        bool sel = i == menuIndex_;
        float y = tl.textY(i);
        if (sel) {
            renderer_.rect(tl.itemX, tl.itemY(i), tl.itemW, TitleLayout::ITEM_H, Color(0.7f, 0.08f, 0.12f, 0.48f));
            renderer_.rect(tl.itemX, tl.itemY(i), 4, TitleLayout::ITEM_H, GOLD);
            renderer_.rectOutline(tl.itemX, tl.itemY(i), tl.itemW, TitleLayout::ITEM_H, 1, Color(1, 0.45f, 0.2f, 0.65f));
        }
        renderer_.textCentered(cx, y, MENU_ITEMS[i], 3, sel ? GOLD : WHITE);
    }
    renderer_.textCentered(cx, panelY + panelH - 42, "UP / DOWN  CHOOSE      ENTER  CONFIRM", 1.5f, DIM);
    renderer_.textRight(width_ - 24, height_ - 22, "WASD MOVE   SPACE JUMP   CTRL CROUCH   J / K / L ATTACK   I STYLE   U DASH", 1.45f, DIM);
    renderer_.text(24, height_ - 22, "F / B + LIGHT  SPECIAL     F + HEAVY  RISING", 1.45f, GOLD);
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
    renderer_.rect(x, y, w, h, Color(0.025f, 0.02f, 0.03f, 0.94f));
    renderer_.rect(x, y, w, 5, c.styleColor);
    renderer_.rect(x, y + h - 2, w, 2, Color(0.75f, 0.12f, 0.16f, 0.8f));
    if (highlighted) {
        renderer_.rect(x, y + 5, 4, h - 7, GOLD);
        renderer_.rectOutline(x - 3, y - 3, w + 6, h + 6, 2, GOLD);
    }
    renderer_.text(x + 10, y + 16, c.name, 2.4f, defeated ? DIM : WHITE);
    renderer_.text(x + 10, y + 42, c.styleName, 1.4f, defeated ? DIM : c.styleColor);
    if (defeated) renderer_.text(x + 10, y + 62, "DEFEATED", 1.6f, RED);
}

void App::renderSelect() {
    const auto& ros = roster();
    int n = (int)ros.size();
    int who = selectFor_;
    int cur = selCursor_[who];
    const Character& c = ros[cur];
    Camera cam;
    cam.eye = Vec3(0, 1.5f, 4.2f); cam.target = Vec3(0, 1.0f, 0); cam.fovDeg = 40;
    renderer_.beginFrame(cam, Color(0.03f, 0.03f, 0.04f));
    renderer_.setDirectionalLight(Vec3(-0.3f, -1.0f, -0.5f), Vec3(0.9f, 0.85f, 0.8f), 0.3f);
    renderer_.setShadowFocus(Vec3(0, 0, 0), 5.0f);
    renderer_.setFog(c.styleColor.rgb() * 0.05f, 0.01f);
    renderer_.addPointLight(Vec3(-2, 2.5f, 2), c.styleColor.rgb() * 1.5f, 8);
    renderer_.addPointLight(Vec3(2, 1.5f, 2), Vec3(0.6f, 0.6f, 0.7f), 8);
    renderer_.draw(titleFloor_, Mat4::identity(), Color(0.7f, 0.7f, 0.8f), 0, 1);
    // lit pedestal, coloured backdrop glow and a spotlight shaft turn the void into a showcase
    {
        const Color sc = c.styleColor;
        renderer_.addPointLight(Vec3(0, 3.2f, 3.0f), Vec3(1.0f, 0.95f, 0.88f) * 0.9f, 12);
        renderer_.addPointLight(Vec3(2.2f, 1.8f, -2.2f), sc.rgb() * 1.4f, 9);
        renderer_.draw(rig_.unitCylinder(), Mat4::translate(Vec3(0, 0.03f, 0)) * Mat4::scale(Vec3(2.6f, 0.06f, 2.6f)), Color(0.07f, 0.07f, 0.09f), 0, 0, false);
        renderer_.draw(rig_.unitCylinder(), Mat4::translate(Vec3(0, 0.065f, 0)) * Mat4::scale(Vec3(2.3f, 0.02f, 2.3f)), sc * 0.9f, 0.9f, 0, false);
        renderer_.draw(rig_.unitCylinder(), Mat4::translate(Vec3(0, 0.08f, 0)) * Mat4::scale(Vec3(2.12f, 0.03f, 2.12f)), Color(0.05f, 0.05f, 0.07f), 0, 0, false);
        renderer_.drawBillboard(Vec3(0, 1.5f, -2.6f), 9.5f, sc.withAlpha(0.20f), Blend::Additive, time_ * 0.1f);
        renderer_.drawBillboard(Vec3(0, 1.3f, -2.4f), 5.0f, sc.withAlpha(0.14f), Blend::Additive, -time_ * 0.15f);
        for (int k = 0; k < 14; ++k) {
            float t = (float)k / 13.0f;
            renderer_.drawBillboard(Vec3(0, 6.0f - 5.8f * t, 0.1f), 1.8f + 1.4f * t, Color(1.0f, 0.95f, 0.85f, 0.02f), Blend::Additive, 0.0f);
        }
    }
    float yaw = selectSpin_ * 0.8f;
    Pose p = idlePose(time_);
    rig_.draw(renderer_, p, Vec3(0, 0, 0), yaw, c, 1.0f);
    if (c.style == StyleType::Stand) {
        Vec3 back(-std::sin(yaw), 0, -std::cos(yaw));
        Vec3 sp = back * 0.9f + Vec3(0.0f, 0.35f + 0.1f * std::sin(time_ * 2.0f), 0);
        rig_.draw(renderer_, standIdlePose(time_), sp, yaw, c, 1.1f, 0.5f, 0, true, false);
        renderer_.drawBillboard(sp + Vec3(0, 1.2f, 0), 2.6f, c.styleColor.withAlpha(0.2f), Blend::Additive, time_);
    } else {
        renderer_.drawBillboard(Vec3(0, 1.0f, 0), 3.0f, c.styleColor.withAlpha(0.15f), Blend::Additive, time_ * 0.7f);
    }

    std::string title = pendingMode_ == FightMode::Hunt ? "CHOOSE YOUR HUNTER" : (who == 0 ? "PLAYER 1  SELECT" : "PLAYER 2  SELECT");
    renderer_.textCentered(width_ * 0.5f, 30, title, 3.5f, GOLD);
    float cardW = 170, cardH = 84, gap = 12;
    float totalW = n * cardW + (n - 1) * gap;
    float x0 = (width_ - totalW) * 0.5f;
    for (int i = 0; i < n; ++i) drawCharacterCard(i, x0 + i * (cardW + gap), height_ - 120, cardW, cardH, i == cur, false);
    // info panel
    float px = 40, py = 128;
    renderer_.rect(24, 96, 390, 385, Color(0.015f, 0.012f, 0.02f, 0.78f));
    renderer_.rectOutline(24, 96, 390, 385, 1, Color(0.7f, 0.12f, 0.16f, 0.7f));
    renderer_.rect(24, 96, 390, 3, c.styleColor);
    renderer_.rect(width_ - 450, 106, 426, 345, Color(0.015f, 0.012f, 0.02f, 0.78f));
    renderer_.rectOutline(width_ - 450, 106, 426, 345, 1, Color(0.7f, 0.12f, 0.16f, 0.7f));
    renderer_.rect(width_ - 450, 106, 426, 3, GOLD);
    renderer_.text(40, 100, pendingMode_ == FightMode::Hunt ? "HUNTER DOSSIER" : "FIGHTER DOSSIER", 1.4f, GOLD);
    renderer_.text(px, py, c.name, 5, WHITE);
    renderer_.text(px, py + 50, c.title, 2, c.styleColor);
    renderer_.text(px, py + 90, c.style == StyleType::Stand ? "STYLE: STAND" : "STYLE: MODE", 2, DIM);
    renderer_.text(px, py + 112, c.styleName, 2.4f, c.styleColor);
    renderer_.text(px, py + 150, "HP " + std::to_string((int)c.hp), 2, DIM);
    // lore, wrapped
    std::vector<std::string> lines; std::string cur_line;
    const std::string lore = localizeText(c.lore, settings_.language);
    std::istringstream words(lore);
    for (std::string word; words >> word;) {
        std::string candidate = cur_line.empty() ? word : cur_line + " " + word;
        if (!cur_line.empty() && renderer_.textWidth(candidate, 1.6f) > 360.0f) {
            lines.push_back(cur_line);
            cur_line = word;
        } else {
            cur_line = candidate;
        }
    }
    if (!cur_line.empty()) lines.push_back(cur_line);
    drawPanelText(px, py + 190, lines, 1.6f, DIM, 1.4f);
    // move list panel on the right
    std::vector<std::string> ml;
    ml.push_back("SPECIAL MOVES");
    for (const Move& m : c.moves) {
        if (m.kind == MoveKind::Special && m.buttons == BTN_LIGHT) {
            std::string mo = m.motion == Motion::QCF ? "F + LIGHT" : m.motion == Motion::QCB ? "B + LIGHT" : "F + HEAVY";
            ml.push_back(m.name.substr(0, m.name.size() - 2) + "  " + mo);
        }
    }
    for (const Move& m : c.moves) if (m.kind == MoveKind::HHA) ml.push_back("HHA  " + m.name + "  F + J+K");
    for (const Move& m : c.moves) if (m.kind == MoveKind::GHA) ml.push_back("GHA  " + m.name + "  F + J+K+L");
    drawPanelText(width_ - 440, 120, ml, 1.6f, WHITE, 1.5f);
    renderer_.textCentered(width_ * 0.5f, height_ - 150, "ARROWS / MOUSE  SELECT     ENTER / CLICK  CONFIRM     ESC  BACK", 1.6f, DIM);
    renderer_.endFrame();
}

// -------------------------------------------------------------- stage select --
namespace {
// Stage list geometry, shared by update (mouse hit-testing) and render so the two never disagree.
struct StageLayout {
    float panelX, panelY, panelW, panelH, itemX, itemW, itemY0;
    static constexpr float ITEM_H = 44, ITEM_STEP = 52;
    float itemY(int i) const { return itemY0 + i * ITEM_STEP; }
};
StageLayout stageLayout(int width, int height) {
    StageLayout l;
    l.panelX = 28; l.panelY = 96; l.panelW = std::min(440.0f, width * 0.38f); l.panelH = height - 150.0f;
    l.itemX = l.panelX + 18; l.itemW = l.panelW - 36; l.itemY0 = l.panelY + 50;
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
        else startFight(selChosen_[0], selChosen_[1], pendingMode_, 1, pick);
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
        renderer_.beginFrame(cam, T.sky);
        hunt_.render(renderer_, rig_, time_);
        // the overview is far away: thin the fog and lift the light so the layout reads
        renderer_.setFog(T.fog, T.fogDensity * 0.06f);
        renderer_.setDirectionalLight(T.sunDir, T.sunColor * 1.8f, std::min(0.6f, T.ambient * 1.9f));
        renderer_.setShadowFocus(c, span * 0.6f);
    } else {
        const Arena& ar = arenas_[shown];
        const float a = stageSpin_ * 0.18f;
        Camera cam;
        cam.target = Vec3(0, 1.3f, 0);
        cam.eye = Vec3(std::sin(a) * 12.5f, 4.2f, std::cos(a) * 12.5f);
        cam.fovDeg = 50; cam.zfar = 300;
        renderer_.beginFrame(cam, ar.info().clear);
        renderer_.setShadowFocus(Vec3(), 12);
        const Character& c0 = roster()[selChosen_[0]];
        const Character& c1 = roster()[selChosen_[1]];
        ar.render(renderer_, rig_.unitBox(), time_, c0.styleColor, c1.styleColor);
        renderer_.addPointLight(cam.eye + Vec3(0, 1.5f, 0), Vec3(1.0f, 0.93f, 0.82f) * 0.8f, 18);
        const float gap = ar.info().shape.spawnGap * 0.5f;
        for (int i = 0; i < 2; ++i)   // the two fighters waiting at their marks
            rig_.draw(renderer_, idlePose(time_ + i * 0.4f), Vec3(i ? gap : -gap, 0, 0), i ? -PI * 0.5f : PI * 0.5f, i ? c1 : c0,
                      1.0f, 1.0f, 0, false, true, 0.01f);
    }

    // ---- menu
    const StageLayout L = stageLayout(width_, height_);
    renderer_.textCentered(width_ * 0.5f, 30, forHunt ? "CHOOSE YOUR HUNTING GROUND" : "CHOOSE THE ARENA", 3.5f, GOLD);
    renderer_.rect(L.panelX, L.panelY, L.panelW, L.panelH, Color(0.015f, 0.012f, 0.02f, 0.84f));
    renderer_.rectOutline(L.panelX, L.panelY, L.panelW, L.panelH, 1, Color(0.7f, 0.12f, 0.16f, 0.8f));
    renderer_.rect(L.panelX, L.panelY, L.panelW, 3, GOLD);
    renderer_.text(L.panelX + 18, L.panelY + 16, forHunt ? "HUNTING GROUNDS" : "ARENAS", 1.5f, GOLD);
    for (int i = 0; i < n; ++i) {
        bool sel = i == stageCursor_;
        const char* name = i < count ? (forHunt ? mazeTheme(i).name : arenaInfo(i).name) : "RANDOM";
        float y = L.itemY(i);
        if (sel) {
            renderer_.rect(L.itemX, y, L.itemW, StageLayout::ITEM_H, Color(0.7f, 0.08f, 0.12f, 0.48f));
            renderer_.rect(L.itemX, y, 4, StageLayout::ITEM_H, GOLD);
            renderer_.rectOutline(L.itemX, y, L.itemW, StageLayout::ITEM_H, 1, Color(1, 0.45f, 0.2f, 0.65f));
        }
        renderer_.text(L.itemX + 16, y + 11, name, 2.4f, sel ? GOLD : WHITE);
    }
    // description, wrapped to the panel
    const std::string blurb = localizeText(random ? "LET FATE DECIDE." : (forHunt ? mazeTheme(shown).blurb : arenaInfo(shown).blurb), settings_.language);
    std::vector<std::string> lines; std::string line;
    std::istringstream words(blurb);
    for (std::string word; words >> word;) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && renderer_.textWidth(candidate, 1.6f) > L.panelW - 44) { lines.push_back(line); line = word; }
        else line = candidate;
    }
    if (!line.empty()) lines.push_back(line);
    if (random) lines.insert(lines.begin(), localizeText(forHunt ? mazeTheme(shown).name : arenaInfo(shown).name, settings_.language));
    drawPanelText(L.panelX + 22, L.itemY(n) + 18, lines, 1.6f, DIM, 1.4f);
    if (!forHunt) {
        const std::string vs = roster()[selChosen_[0]].name + "  VS  " + roster()[selChosen_[1]].name;
        renderer_.textRight(width_ - 32, height_ - 86, vs, 3.0f, WHITE);
    } else {
        renderer_.textRight(width_ - 32, height_ - 86, roster()[selChosen_[0]].name, 3.0f, roster()[selChosen_[0]].styleColor);
    }
    renderer_.textCentered(width_ * 0.5f, height_ - 36, "ARROWS / MOUSE  SELECT     ENTER / CLICK  CONFIRM     ESC  BACK", 1.6f, DIM);
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
            if (!input_.mouseIn(width_ * 0.5f - 180, height_ * 0.5f + i * 40 - 6, 360, 33)) continue;
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
    if (huntEngaged_ >= 0) {
        // dramatic zoom on the found opponent
        const HuntOpponent& o = hunt_.opponents()[huntEngaged_];
        float t = smoothstep(0, 1, huntEngageTimer_ / 1.2f);
        Vec3 toP = normalize(hunt_.playerPos() - o.pos);
        Vec3 side = cross(toP, Vec3(0, 1, 0));
        Vec3 eye = o.pos + toP * 2.4f + side * 1.1f + Vec3(0, 1.4f, 0);
        cam.eye = lerp(cam.eye, eye, t);
        cam.target = lerp(cam.target, o.pos + Vec3(0, 1.3f, 0), t);
        cam.fovDeg = lerp(58.0f, 40.0f, t);
    }
    renderer_.beginFrame(cam, hunt_.maze().theme().sky);
    hunt_.render(renderer_, rig_, hunt_.worldTime());

    const auto& ros = roster();
    float cx = width_ * 0.5f;
    // --- HUD: Death Sense
    float level = hunt_.senseLevel();
    float pulse = hunt_.heartPulse();
    float hx = 40, hy = 40;
    renderer_.rect(24, 24, 330, 84, Color(0.012f, 0.01f, 0.016f, 0.84f));
    renderer_.rectOutline(24, 24, 330, 84, 1, Color(0.7f, 0.12f, 0.16f, 0.75f));
    renderer_.rect(24, 24, 3, 84, RED);
    renderer_.text(hx, hy, "DEATH SENSE", 2, DIM);
    renderer_.rect(hx, hy + 24, 260, 14, Color(0.1f, 0.1f, 0.12f, 0.8f));
    renderer_.rect(hx, hy + 24, 260 * level, 14, mix(Color(0.3f, 0.3f, 0.9f), Color(1.0f, 0.1f, 0.15f), level));
    float hs = 22 + 16 * pulse * (0.4f + level);
    renderer_.rect(hx + 290 - hs * 0.5f, hy + 31 - hs * 0.5f, hs, hs, Color(1.0f, 0.15f, 0.2f, 0.5f + 0.5f * pulse));
    if (level > 0.22f && hunt_.senseDirection() >= 0) {
        // direction chevron relative to the camera
        const float dirYaw[4] = {0.0f, PI * 0.5f, PI, -PI * 0.5f};
        float rel = wrapAngle(dirYaw[hunt_.senseDirection()] - hunt_.cameraYaw());
        const char* label = std::fabs(rel) < 0.4f ? "AHEAD" : (std::fabs(rel) > 2.7f ? "BEHIND" : (rel > 0 ? "RIGHT" : "LEFT"));
        renderer_.text(hx, hy + 48, std::string("TRAIL: ") + label, 1.8f, Color(1.0f, 0.5f, 0.5f, 0.5f + 0.5f * level));
    }
    if (hunt_.anyOpponentHunting()) renderer_.textCentered(cx, 60, "YOU ARE BEING HUNTED", 2.2f, RED * (0.7f + 0.3f * std::sin(time_ * 10)));

    // roster status
    float rx = width_ - 220, ry = 40;
    renderer_.rect(rx - 12, ry - 16, 200, 150, Color(0.012f, 0.01f, 0.016f, 0.84f));
    renderer_.rectOutline(rx - 12, ry - 16, 200, 150, 1, Color(0.7f, 0.12f, 0.16f, 0.75f));
    renderer_.text(rx, ry, "TARGETS", 2, DIM);
    for (size_t i = 0; i < hunt_.opponents().size(); ++i) {
        const HuntOpponent& o = hunt_.opponents()[i];
        const Character& c = ros[o.charId];
        float y = ry + 26 + (float)i * 22;
        renderer_.rect(rx, y + 2, 10, 10, o.defeated ? Color(0.3f, 0.3f, 0.3f) : c.styleColor);
        renderer_.text(rx + 18, y, c.name, 1.7f, o.defeated ? DIM : WHITE);
        if (o.defeated) renderer_.text(rx + 110, y, "DOWN", 1.5f, RED);
    }
    // player
    renderer_.text(40, height_ - 50, ros[huntPlayerChar_].name, 2.4f, ros[huntPlayerChar_].styleColor);
    renderer_.text(40, height_ - 28, "WASD / ARROWS MOVE   MOUSE LOOK   SHIFT RUN   ENTER ENGAGE   ESC PAUSE", 1.5f, DIM);

    if (huntBannerTimer_ > 0) {
        float a = clampf(huntBannerTimer_, 0, 1);
        renderer_.textCentered(cx, height_ * 0.22f, "THE HUNT BEGINS", 5, GOLD.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.22f + 50, std::to_string(hunt_.remaining()) + " OPPONENTS HIDE IN THESE HALLS. FIND THEM.", 2, WHITE.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.22f + 76, "FOLLOW YOUR DEATH SENSE. WALK INTO THEM TO FIGHT.", 2, DIM.withAlpha(a));
    }
    if (hunt_.engageCandidate() >= 0 && huntEngaged_ < 0) {
        const Character& c = ros[hunt_.opponents()[hunt_.engageCandidate()].charId];
        renderer_.textCentered(cx, height_ * 0.68f, "ENGAGE " + c.name + "   [ENTER / ATTACK]", 2.4f, GOLD * (0.8f + 0.2f * std::sin(time_ * 8)));
    } else if (hunt_.nearestVisible() >= 0 && huntEngaged_ < 0) {
        const Character& c = ros[hunt_.opponents()[hunt_.nearestVisible()].charId];
        renderer_.textCentered(cx, height_ * 0.68f, c.name + " IS NEAR", 2, c.styleColor);
    }
    if (huntEngaged_ >= 0) {
        const Character& c = ros[hunt_.opponents()[huntEngaged_].charId];
        float t = huntEngageTimer_;
        float a = clampf(t * 2.0f, 0, 1);
        renderer_.rect(0, height_ * 0.3f, width_, 120, Color(0, 0, 0, 0.6f * a));
        renderer_.textCentered(cx, height_ * 0.3f + 18, "OPPONENT FOUND", 3, WHITE.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.3f + 54, c.name + "  -  " + c.title, 4, c.styleColor.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.3f + 96, c.styleName, 2, DIM.withAlpha(a));
    }
    if (huntPaused_) {
        renderer_.rect(0, 0, width_, height_, Color(0.008f, 0.005f, 0.012f, 0.76f));
        renderer_.rect(cx - 220, height_ * 0.34f, 440, 290, Color(0.02f, 0.012f, 0.02f, 0.96f));
        renderer_.rectOutline(cx - 220, height_ * 0.34f, 440, 290, 2, RED.withAlpha(0.9f));
        renderer_.rect(cx - 220, height_ * 0.34f, 440, 4, GOLD);
        renderer_.textCentered(cx, height_ * 0.35f, "PAUSED", 5, WHITE);
        const char* items[3] = {"RESUME", "SETTINGS", "QUIT TO TITLE"};
        for (int i = 0; i < 3; ++i) {
            float y = height_ * 0.5f + i * 40;
            if (i == huntPauseIndex_) {
                renderer_.rect(cx - 170, y - 5, 340, 32, Color(0.65f, 0.06f, 0.1f, 0.5f));
                renderer_.rect(cx - 170, y - 5, 3, 32, GOLD);
            }
            renderer_.textCentered(cx, y, items[i], 3, i == huntPauseIndex_ ? GOLD : WHITE);
        }
        renderer_.textCentered(cx, height_ * 0.85f, "UP / DOWN / MOUSE  CHOOSE     ENTER / CLICK  CONFIRM     ESC  RESUME", 1.6f, DIM);
    }
    if (huntComplete_) {
        float a = clampf(huntCompleteTimer_, 0, 1);
        renderer_.rect(0, 0, width_, height_, Color(0, 0, 0, 0.7f * a));
        renderer_.textCentered(cx, height_ * 0.35f, "HUNT COMPLETE", 7, GOLD.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.35f + 80, ros[huntPlayerChar_].name + " STANDS ALONE IN THE HALLS", 2.5f, WHITE.withAlpha(a));
        if (huntCompleteTimer_ > 1.5f) renderer_.textCentered(cx, height_ * 0.7f, "PRESS ENTER OR CLICK", 2, DIM);
    }
    renderer_.endFrame();
}

} // namespace dc
