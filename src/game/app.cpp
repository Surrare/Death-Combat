#include "game/app.h"
#include "core/png.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <algorithm>
#include <filesystem>

namespace dc {

namespace {
const Color WHITE(1, 1, 1), GOLD(1.0f, 0.82f, 0.3f), RED(0.95f, 0.2f, 0.2f), DIM(0.55f, 0.55f, 0.6f), CYAN(0.4f, 0.9f, 1.0f);
const char* MENU_ITEMS[] = {"HUNT MODE", "VERSUS CPU", "VERSUS 2P", "SETTINGS", "QUIT"};
}

bool App::init(GLFWwindow* window, const AppOptions& opts) {
    window_ = window;
    opts_ = opts;
    if (!renderer_.init()) return false;
    if (!rig_.init()) return false;
    settings_.load();
    input_.attach(window, &settings_);
    applyVideoSettings();
    audio_.init();
    fxRng_ = Rng(opts.seed ? opts.seed : (uint64_t)glfwGetTime() * 1000 + 7);
    buildArena();
    {
        MeshBuilder b;
        b.addPlane(Vec3(0, 0, 0), 40, 40, 20, Vec3(0.2f, 0.18f, 0.22f), Vec3(0.16f, 0.14f, 0.18f));
        titleFloor_.upload(b);
        b.clear();
        b.addSphere(Vec3(0, 0, 0), 0.5f, 8, 12, Vec3(1, 1, 1));
        projectileMesh_.upload(b);
    }
    if (opts.demo == 1) { startFight(0, 1, FightMode::Demo, 2); }
    else if (opts.demo == 2) { startHunt(0); }
    else if (opts.demo == 4) { openSettings(Scene::Title); }
    else if (opts.demo == 3) { scene_ = Scene::Select; pendingMode_ = FightMode::Hunt; selectFor_ = 0; }
    return true;
}

void App::takeScreenshot(const std::string& path) {
    std::vector<uint8_t> px((size_t)width_ * height_ * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    if (writePng(path.c_str(), width_, height_, px.data())) std::printf("screenshot saved: %s\n", path.c_str());
}

SimInput App::padToSim(const PadState& p) const {
    SimInput s;
    s.dirX = p.dirX; s.dirY = p.dirY;
    s.held = p.held; s.pressed = p.pressed;
    return s;
}

bool App::project(const Vec3& p, float& sx, float& sy) const {
    Vec4 clip = renderer_.projMatrix() * (renderer_.viewMatrix() * Vec4(p, 1.0f));
    if (clip.w <= 0.001f) return false;
    float nx = clip.x / clip.w, ny = clip.y / clip.w;
    sx = (nx * 0.5f + 0.5f) * width_;
    sy = (1.0f - (ny * 0.5f + 0.5f)) * height_;
    return true;
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
    for (size_t i = 0; i < lines.size(); ++i) renderer_.text(x, y + (float)i * 8.0f * scale * lineGap, lines[i], scale, col);
}

// --------------------------------------------------------------------- frame --
void App::frame(float dt, int w, int h) {
    width_ = w; height_ = h;
    renderer_.resize(w, h);
    input_.poll();
    time_ += dt;

    audio_.setVolumes(settings_.master, settings_.music, settings_.sfx);
    if (menuSoundsActive()) {
        const PadState& m = input_.menu();
        bool capturing = scene_ == Scene::Settings && settingsCapturing_;
        if (!capturing) {
            if (m.justPressed(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) audio_.play(Sfx::UiMove);
            if (m.justPressed(BTN_START | BTN_LIGHT)) audio_.play(Sfx::UiConfirm);
            if (m.justPressed(BTN_BACK)) audio_.play(Sfx::UiBack);
        }
    }

    switch (scene_) {
    case Scene::Title: updateTitle(dt); renderTitle(); break;
    case Scene::Select: updateSelect(dt); renderSelect(); break;
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
    case Scene::Title: case Scene::Select: case Scene::Settings: return true;
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
    if (p.justPressed(BTN_START) || p.justPressed(BTN_LIGHT)) {
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
    // HUD
    float cx = width_ * 0.5f;
    float pulse = 0.85f + 0.15f * std::sin(titleTime_ * 3.0f);
    renderer_.textCentered(cx + 4, 64 + 4, "DEATH COMBAT", 9, Color(0, 0, 0, 0.6f));
    renderer_.textCentered(cx, 64, "DEATH COMBAT", 9, Color(0.95f, 0.15f, 0.2f) * pulse);
    renderer_.textCentered(cx, 150, "SEEK YOUR OPPONENT.  STAND AND FIGHT.", 2, DIM);
    float my = height_ * 0.5f;
    for (int i = 0; i < 5; ++i) {
        bool sel = i == menuIndex_;
        float y = my + i * 44;
        if (sel) renderer_.rect(cx - 170, y - 8, 340, 38, Color(0.9f, 0.15f, 0.2f, 0.35f));
        renderer_.textCentered(cx, y, MENU_ITEMS[i], 3, sel ? GOLD : WHITE);
    }
    renderer_.textCentered(cx, my + 5 * 44 + 10, "UP / DOWN  CHOOSE      ENTER  CONFIRM", 1.8f, DIM);
    auto K = [&](Action a) { return settings_.keyLabel(a); };
    renderer_.text(20, height_ - 100, "MOVE  " + K(Action::Up) + " " + K(Action::Left) + " " + K(Action::Down) + " " + K(Action::Right) + " / LEFT STICK      LIGHT " + K(Action::Light) + "   MEDIUM " + K(Action::Medium) + "   HEAVY " + K(Action::Heavy), 1.6f, DIM);
    renderer_.text(20, height_ - 80, "STAND / MODE  " + K(Action::Style) + "      DASH / EVADE  " + K(Action::Dash) + "      TAUNT  " + K(Action::Taunt), 1.6f, DIM);
    renderer_.text(20, height_ - 60, "MENUS  ARROWS / WASD + ENTER      PAUSE  ESC      SCREENSHOT  F12", 1.6f, DIM);
    renderer_.text(20, height_ - 40, "GAMEPAD: X LIGHT  Y MEDIUM  B HEAVY  A STAND  RB DASH  LB TAUNT", 1.6f, DIM);
    renderer_.textRight(width_ - 20, height_ - 30, "OPENGL 3.3  /  LINUX + WINDOWS", 1.5f, DIM);
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
    if (m.justPressed(BTN_BACK)) {
        if (who == 1) { selectFor_ = 0; return; }   // step back to player 1's pick
        scene_ = Scene::Title; return;
    }
    if (m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT)) {
        selChosen_[who] = selCursor_[who];
        if (pendingMode_ == FightMode::Hunt) { startHunt(selChosen_[0]); return; }
        if (pendingMode_ == FightMode::VsCpu) {
            int cpu = fxRng_.range(0, n - 2);
            if (cpu >= selChosen_[0]) cpu++;
            startFight(selChosen_[0], cpu, FightMode::VsCpu, 1);
            return;
        }
        if (who == 0) { selectFor_ = 1; selCursor_[1] = (selCursor_[0] + 1) % n; }
        else startFight(selChosen_[0], selChosen_[1], FightMode::Versus, 1);
    }
}

void App::drawCharacterCard(int id, float x, float y, float w, float h, bool highlighted, bool defeated) {
    const Character& c = roster()[id];
    renderer_.rect(x, y, w, h, Color(0.08f, 0.08f, 0.1f, 0.9f));
    renderer_.rect(x, y, w, 6, c.styleColor);
    if (highlighted) renderer_.rectOutline(x - 3, y - 3, w + 6, h + 6, 3, GOLD);
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
    float px = 40, py = 110;
    renderer_.text(px, py, c.name, 5, WHITE);
    renderer_.text(px, py + 50, c.title, 2, c.styleColor);
    renderer_.text(px, py + 90, c.style == StyleType::Stand ? "STYLE: STAND" : "STYLE: MODE", 2, DIM);
    renderer_.text(px, py + 112, c.styleName, 2.4f, c.styleColor);
    renderer_.text(px, py + 150, "HP " + std::to_string((int)c.hp), 2, DIM);
    // lore, wrapped
    std::vector<std::string> lines; std::string cur_line;
    for (size_t i = 0; i < c.lore.size(); ++i) {
        cur_line += c.lore[i];
        if (cur_line.size() > 34 && c.lore[i] == ' ') { lines.push_back(cur_line); cur_line.clear(); }
    }
    if (!cur_line.empty()) lines.push_back(cur_line);
    drawPanelText(px, py + 190, lines, 1.6f, DIM, 1.4f);
    // move list panel on the right
    std::vector<std::string> ml;
    ml.push_back("SPECIAL MOVES");
    for (const Move& m : c.moves) {
        if (m.kind == MoveKind::Special && m.buttons == BTN_LIGHT) {
            std::string mo = m.motion == Motion::QCF ? "D,DF,F + ATK" : m.motion == Motion::QCB ? "D,DB,B + ATK" : "F,D,DF + ATK";
            ml.push_back(m.name.substr(0, m.name.size() - 2) + "  " + mo);
        }
    }
    for (const Move& m : c.moves) if (m.kind == MoveKind::HHA) ml.push_back("HHA  " + m.name + "  D,DF,F + 2 ATK");
    for (const Move& m : c.moves) if (m.kind == MoveKind::GHA) ml.push_back("GHA  " + m.name + "  D,DF,F + 3 ATK");
    drawPanelText(width_ - 440, 120, ml, 1.6f, WHITE, 1.5f);
    renderer_.textCentered(width_ * 0.5f, height_ - 150, "ARROWS  CHOOSE FIGHTER     ENTER  CONFIRM     ESC  BACK", 1.6f, DIM);
    renderer_.endFrame();
}

// ---------------------------------------------------------------------- hunt --
void App::startHunt(int playerChar) {
    huntPlayerChar_ = playerChar;
    huntOrder_.clear();
    const auto& ros = roster();
    for (int i = 0; i < (int)ros.size(); ++i) if (i != playerChar) huntOrder_.push_back(i);
    uint64_t seed = opts_.seed ? opts_.seed : (uint64_t)(glfwGetTime() * 1000.0) + 17;
    hunt_.init(playerChar, huntOrder_, seed);
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
        if (huntCompleteTimer_ > 1.5f && (pad.justPressed(BTN_START) || pad.justPressed(BTN_LIGHT) || pad.justPressed(BTN_BACK))) scene_ = Scene::Title;
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
        if (pad.justPressed(BTN_START) || pad.justPressed(BTN_LIGHT)) {
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
            startFight(huntPlayerChar_, hunt_.opponents()[huntEngaged_].charId, FightMode::Hunt, difficulty);
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
    renderer_.beginFrame(cam, Color(0.02f, 0.015f, 0.03f));
    hunt_.render(renderer_, rig_, hunt_.worldTime());

    const auto& ros = roster();
    float cx = width_ * 0.5f;
    // --- HUD: Death Sense
    float level = hunt_.senseLevel();
    float pulse = hunt_.heartPulse();
    float hx = 40, hy = 40;
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
        renderer_.rect(0, 0, width_, height_, Color(0, 0, 0, 0.6f));
        renderer_.textCentered(cx, height_ * 0.35f, "PAUSED", 5, WHITE);
        const char* items[3] = {"RESUME", "SETTINGS", "QUIT TO TITLE"};
        for (int i = 0; i < 3; ++i) renderer_.textCentered(cx, height_ * 0.5f + i * 40, items[i], 3, i == huntPauseIndex_ ? GOLD : WHITE);
        renderer_.textCentered(cx, height_ * 0.85f, "UP / DOWN  CHOOSE     ENTER  CONFIRM     ESC  RESUME", 1.6f, DIM);
    }
    if (huntComplete_) {
        float a = clampf(huntCompleteTimer_, 0, 1);
        renderer_.rect(0, 0, width_, height_, Color(0, 0, 0, 0.7f * a));
        renderer_.textCentered(cx, height_ * 0.35f, "HUNT COMPLETE", 7, GOLD.withAlpha(a));
        renderer_.textCentered(cx, height_ * 0.35f + 80, ros[huntPlayerChar_].name + " STANDS ALONE IN THE HALLS", 2.5f, WHITE.withAlpha(a));
        if (huntCompleteTimer_ > 1.5f) renderer_.textCentered(cx, height_ * 0.7f, "PRESS ENTER", 2, DIM);
    }
    renderer_.endFrame();
}

} // namespace dc
