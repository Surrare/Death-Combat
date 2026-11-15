// Fight scene presentation: arena, camera, smooth pose blending, effects, HUD, and Developer Mode tools.
#include "game/app.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <cctype>

namespace dc {

namespace {
const Color WHITE(0.95f, 0.96f, 0.98f);
const Color GOLD(0.88f, 0.74f, 0.42f);
const Color RED(0.85f, 0.24f, 0.26f);
const Color DIM(0.56f, 0.59f, 0.65f);
const Color CYAN(0.38f, 0.82f, 0.95f);
const Color GREEN(0.32f, 0.88f, 0.48f);
const Color UI_PANEL(0.035f, 0.04f, 0.055f, 0.90f);
const Color UI_PANEL_HI(0.11f, 0.13f, 0.18f, 0.95f);
const Color UI_BORDER(0.22f, 0.25f, 0.32f, 0.85f);
const Color UI_SUBTEXT(0.68f, 0.72f, 0.78f, 1.0f);

constexpr float ARENA_SURFACE_Y = 0.01f;

std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

std::string displayName(const std::string& n) {
    if (n.size() > 2 && n[n.size() - 2] == ' ' && (n.back() == 'L' || n.back() == 'M' || n.back() == 'H'))
        return upper(n.substr(0, n.size() - 2));
    return upper(n);
}

const char* stateName(FState s) {
    switch (s) {
    case FState::Intro: return "INTRO";
    case FState::Idle: return "IDLE";
    case FState::Walk: return "WALK";
    case FState::Crouch: return "CROUCH";
    case FState::Jump: return "JUMP";
    case FState::Land: return "LAND";
    case FState::Dash: return "DASH";
    case FState::Backdash: return "BACKDASH";
    case FState::Sidestep: return "SIDESTEP";
    case FState::Attack: return "ATTACK";
    case FState::Block: return "BLOCK";
    case FState::CrouchBlock: return "CROUCH_BLK";
    case FState::Hit: return "HIT";
    case FState::CrouchHit: return "CROUCH_HIT";
    case FState::AirHit: return "AIR_HIT";
    case FState::Knockdown: return "KNOCKDOWN";
    case FState::GetUp: return "GETUP";
    case FState::Thrown: return "THROWN";
    case FState::StyleBreak: return "STYLE_BRK";
    case FState::Clash: return "CLASH";
    case FState::KO: return "KO";
    case FState::Win: return "WIN";
    case FState::Taunt: return "TAUNT";
    }
    return "UNKNOWN";
}
} // namespace

void App::startFight(int charA, int charB, FightMode mode, int difficulty, int arena) {
    const auto& ros = roster();
    fightMode_ = mode;
    arenaId_ = arena >= 0 ? arena % ARENA_COUNT : fxRng_.range(0, ARENA_COUNT - 1);
    sim_.setArena(arenaInfo(arenaId_).shape);
    sim_.setRoundSeconds(settings_.roundSeconds());
    sim_.setDamageScale(settings_.damageScale());
    sim_.setHitstopEnabled(settings_.hitstopEnabled);
    sim_.setEasyControls(0, settings_.easyControls[0]);
    sim_.setEasyControls(1, settings_.easyControls[1]);
    sim_.init(&ros[charA], &ros[charB], settings_.roundsToWin);
    p1Ai_ = mode == FightMode::Demo;
    p2Ai_ = mode != FightMode::Versus;
    ai_[0].reset(2, 1234);
    ai_[1].reset(difficulty, 99 + (uint64_t)charB * 31);
    simAccum_ = 0; paused_ = false; pauseIndex_ = 0; showMoveList_ = false; resultIndex_ = 0; resultTimer_ = 0;
    particles_.clear(); floats_.clear(); banners_.clear();
    fightCamInit_ = false; standPosInit_ = false; fightTime_ = 0; slowmo_ = 1.0f;
    fightCamYaw_ = 0; splitAmount_ = 0;
    devFramePause_ = false; devStepOnce_ = false;
    for (int i = 0; i < 2; ++i) {
        displayYaw_[i] = sim_.fighter(i).yaw();
        displayPoseInit_[i] = false;
    }
    scene_ = Scene::Fight;
    input_.setMouseCaptured(false);
    processEvents();
}

// Movement on screen is camera-relative; the simulation wants it relative to the fighter (toward the opponent / sideways).
SimInput App::padToSim(const PadState& p, int who) const {
    SimInput s;
    const Fighter& f = sim_.fighter(who);
    const Vec3 camF(-std::sin(fightCamYaw_), 0, -std::cos(fightCamYaw_));   // the camera looks this way along the floor
    const Vec3 camR(-camF.z, 0, camF.x);
    Vec3 w = camR * p.axisX + camF * p.axisY;
    float l = w.length();
    if (l > 1.0f) w = w / l;
    s.moveF = dot(w, f.fwd);
    s.moveS = dot(w, f.right());
    s.dirY = (p.held & BTN_CROUCH) ? -1 : ((p.held & BTN_JUMP) ? 1 : 0);
    s.held = p.held; s.pressed = p.pressed;
    return s;
}

namespace {
Vec3 flatV(Vec3 v) { v.y = 0; return v; }
// Pull the eye back inside the arena's camera circle along the view ray, raising it a little to keep the framing.
void clampEye(Camera& c, float radius) {
    Vec3 t = flatV(c.target), d = flatV(c.eye - c.target);
    float dd = dot(d, d);
    if (dd < 1e-6f || flatV(c.eye).length() <= radius) return;
    float td = dot(t, d), tt = dot(t, t) - radius * radius;
    float disc = td * td - dd * tt;
    if (disc < 0) return;
    float s = clampf((-td + std::sqrt(disc)) / dd, 0.25f, 1.0f);
    c.eye = c.target + (c.eye - c.target) * s + Vec3(0, (1.0f - s) * 2.2f, 0);
}
}

void App::updateFightCamera() {
    const Fighter& a = sim_.fighter(0);
    const Fighter& b = sim_.fighter(1);
    const float dt = frameDt_;
    Vec3 pa = flatV(a.pos), pb = flatV(b.pos);
    Vec3 axis = pb - pa;
    float sep = axis.length();
    if (sep > 0.3f && !sim_.clash().active) {
        Vec3 n = Vec3(-axis.z, 0, axis.x) / sep;
        Vec3 cur(std::sin(fightCamYaw_), 0, std::cos(fightCamYaw_));
        if (dot(n, cur) < 0) n = -n;   // stay on the side we are already on: no sudden half turns
        float want = std::atan2(n.x, n.z);
        float rate = 2.6f * clampf((sep - 0.6f) / 1.6f, 0.0f, 1.0f);   // hold still while they are on top of each other
        fightCamYaw_ += wrapAngle(want - fightCamYaw_) * (1.0f - std::exp(-rate * dt));
    }
    const float ZMAX = 9.0f, R = ZMAX * 0.5f;
    float highY = std::max(0.0f, std::max(a.pos.y, b.pos.y));
    float midY = std::max(-0.4f, (a.pos.y + b.pos.y) * 0.5f);
    float zoom = clampf(std::max(sep, highY * 2.2f), 2.4f, ZMAX);
    Vec3 mid = (pa + pb) * 0.5f;
    const bool versus = fightMode_ == FightMode::Versus;
    Vec3 focus = mid;
    if (!versus && fightMode_ != FightMode::Demo && sep > 2 * R) focus = pa + normalize(mid - pa) * R;

    const Vec3 camDir(std::sin(fightCamYaw_), 0, std::cos(fightCamYaw_));
    Camera want;
    want.target = focus + Vec3(0, 1.05f + midY * 0.45f + highY * 0.2f + zoom * 0.1f, 0);
    want.eye = want.target + camDir * (4.9f + zoom * 0.62f) + Vec3(0, 0.75f + zoom * 0.1f, 0);
    want.fovDeg = clampf(42.0f + (settings_.fovDeg - 55) * 0.65f, 34.0f, 56.0f);
    if (sim_.phase() == Phase::Intro) {
        float t = (float)sim_.phaseTimer() / 150.0f;
        float orbit = std::sin(t * 2.0f) * 0.55f;
        Vec3 d(std::sin(fightCamYaw_ + orbit), 0, std::cos(fightCamYaw_ + orbit));
        want.eye = want.target + d * (5.5f + (1.0f - t) * 2.0f) + Vec3(0, 1.2f - t * 0.5f, 0);
    }
    const float camR = arenaInfo(arenaId_).cameraRadius;
    clampEye(want, camR);
    if (!fightCamInit_) { fightCam_ = want; fightCamInit_ = true; }
    else {
        float k = 1.0f - std::pow(1.0f - 0.15f, dt * 60.0f);
        fightCam_.eye = lerp(fightCam_.eye, want.eye, k);
        fightCam_.target = lerp(fightCam_.target, want.target, k);
        fightCam_.fovDeg = lerp(fightCam_.fovDeg, want.fovDeg, k);
    }
    fightCam_.znear = 0.3f;

    viewCam_[0] = viewCam_[1] = fightCam_;
    splitActive_ = false;
    if (versus && sep > 2 * R) {
        Vec3 shift[2] = {pa + normalize(mid - pa) * R - mid, pb + normalize(mid - pb) * R - mid};
        for (int i = 0; i < 2; ++i) {
            viewCam_[i].eye += shift[i]; viewCam_[i].target += shift[i];
            clampEye(viewCam_[i], camR + R);
        }
        const float aspect = (float)std::max(width_, 1) / (float)std::max(height_, 1);
        const Mat4 pv = Mat4::perspective(radians(fightCam_.fovDeg), aspect, fightCam_.znear, fightCam_.zfar) *
                        Mat4::lookAt(fightCam_.eye, fightCam_.target, Vec3(0, 1, 0));
        auto screen = [&](Vec3 p, Vec2& o) {
            Vec4 c = pv * Vec4(p, 1.0f);
            if (c.w <= 1e-3f) return false;
            o = Vec2(c.x / c.w * 0.5f * width_, -c.y / c.w * 0.5f * height_);
            return true;
        };
        Vec2 sa, sb;
        if (screen(a.pos + Vec3(0, 1, 0), sa) && screen(b.pos + Vec3(0, 1, 0), sb)) {
            Vec2 n = sb - sa;
            if (n.length() > 1.0f) splitNormal_ = n * (1.0f / n.length());
        }
        splitActive_ = true;
    }
    float wantSplit = splitActive_ ? clampf((sep - 2 * R) / 1.8f, 0.0f, 1.0f) : 0.0f;
    splitAmount_ = lerp(splitAmount_, wantSplit, 1.0f - std::exp(-8.0f * dt));
}

void App::spawnSparks(Vec3 pos, int n, Color a, Color b, float speed, float size, float life, Blend blend, float gravity) {
    for (int i = 0; i < n; ++i) {
        Particle p;
        p.pos = pos + Vec3(fxRng_.uniform(-0.1f, 0.1f), fxRng_.uniform(-0.1f, 0.1f), fxRng_.uniform(-0.1f, 0.1f));
        Vec3 dir(fxRng_.uniform(-1, 1), fxRng_.uniform(-1, 1), fxRng_.uniform(-1, 1));
        p.vel = normalize(dir) * (speed * fxRng_.uniform(0.3f, 1.0f));
        p.life = p.maxLife = life * fxRng_.uniform(0.6f, 1.0f);
        p.size = size * fxRng_.uniform(0.6f, 1.3f);
        p.col = mix(a, b, fxRng_.uniform());
        p.blend = blend; p.gravity = gravity;
        particles_.push_back(p);
    }
}

void App::processEvents() {
    for (const FightEvent& e : sim_.events()) {
        const Character* c = e.player >= 0 ? sim_.fighter(e.player).ch : nullptr;
        Color sc = c ? c->styleColor : GOLD;
        switch (e.type) {
        case FightEvent::Hit: {
            float s = e.strength;
            spawnSparks(e.pos, (int)(10 * s), Color(1, 1, 0.8f), Color(1, 0.5f, 0.2f), 6.0f * s, 0.12f, 0.45f, Blend::Additive, 9.0f);
            Particle flash; flash.pos = e.pos; flash.life = flash.maxLife = 0.12f; flash.size = 0.9f * s; flash.col = Color(1, 1, 1, 0.9f); flash.growth = 4.0f;
            particles_.push_back(flash);
            Particle ring; ring.pos = e.pos; ring.life = ring.maxLife = 0.25f; ring.size = 0.3f; ring.growth = 6.0f * s; ring.col = sc.withAlpha(0.6f);
            particles_.push_back(ring);
            hudFlash_ = std::max(hudFlash_, 0.25f * s);
            audio_.play(s >= 1.5f ? Sfx::HitHeavy : Sfx::HitLight, 0.8f + 0.2f * std::min(s, 2.0f), 0.92f + 0.16f * fxRng_.range(0, 100) / 100.0f);
            break;
        }
        case FightEvent::Block:
            audio_.play(Sfx::Block);
            spawnSparks(e.pos, 6, Color(0.6f, 0.8f, 1), Color(1, 1, 1), 3.5f, 0.09f, 0.35f, Blend::Additive, 4.0f);
            if (!e.text.empty()) addFloat(e.pos + Vec3(0, 0.6f, 0), e.text, Color(0.7f, 0.85f, 1), 0.8f, 2);
            break;
        case FightEvent::Throw:
            audio_.play(Sfx::Throw);
            spawnSparks(e.pos, 12, Color(1, 0.8f, 0.6f), Color(1, 0.4f, 0.3f), 5.0f, 0.14f, 0.5f, Blend::Additive, 8.0f);
            break;
        case FightEvent::Spark:
            spawnSparks(e.pos, (int)(8 * e.strength), sc, WHITE, 5.0f, 0.1f, 0.4f, Blend::Additive, 5.0f);
            break;
        case FightEvent::Dust:
            audio_.play(Sfx::Whoosh, 0.4f * std::min(e.strength, 1.5f));
            spawnSparks(e.pos + Vec3(0, 0.1f, 0), (int)(6 * e.strength), Color(0.55f, 0.5f, 0.45f, 0.5f), Color(0.4f, 0.37f, 0.33f, 0.4f), 1.8f, 0.25f, 0.6f, Blend::Alpha, -1.5f);
            for (size_t i = particles_.size() - (size_t)(6 * e.strength); i < particles_.size(); ++i) { particles_[i].growth = 0.8f; particles_[i].vel.y = std::fabs(particles_[i].vel.y) * 0.4f; }
            break;
        case FightEvent::Land:
            audio_.play(Sfx::Land);
            spawnSparks(e.pos + Vec3(0, 0.1f, 0), 14, Color(0.55f, 0.5f, 0.45f, 0.6f), Color(0.4f, 0.37f, 0.33f, 0.4f), 3.0f, 0.3f, 0.7f, Blend::Alpha, -1.0f);
            for (size_t i = particles_.size() - 14; i < particles_.size(); ++i) { particles_[i].growth = 1.0f; particles_[i].vel.y = std::fabs(particles_[i].vel.y) * 0.5f; }
            break;
        case FightEvent::Text:
            if (e.player < 0) addBanner(e.text, GOLD, 1.2f, 6);
            else addFloat(e.pos, e.text, sc, 1.1f, 2.0f);
            break;
        case FightEvent::MoveName: {
            Color col = e.strength >= 3 ? Color(1, 0.3f, 0.2f) : (e.strength >= 2 ? GOLD : sc);
            addFloat(e.pos + Vec3(0, 2.4f, 0), displayName(e.text), col, e.strength >= 2 ? 1.8f : 1.0f, e.strength >= 2 ? 2.8f : 1.9f);
            if (e.strength >= 2) {
                audio_.play(Sfx::HeartHeat, e.strength >= 3 ? 1.0f : 0.8f, e.strength >= 3 ? 0.8f : 1.0f);
                addBanner(e.strength >= 3 ? "GREAT HEAT ATTACK" : "HEART HEAT ATTACK", col, 1.4f, 3.8f, 0.25f);
                const Fighter& f = sim_.fighter(e.player);
                spawnSparks(f.pos + Vec3(0, 1.0f, 0), 40, sc, WHITE, 7.0f, 0.18f, 0.9f, Blend::Additive, 0.0f);
                Particle flash; flash.pos = f.pos + Vec3(0, 1.0f, 0); flash.life = flash.maxLife = 0.35f; flash.size = 2.5f; flash.col = sc.withAlpha(0.8f); flash.growth = 8.0f;
                particles_.push_back(flash);
                hudFlash_ = 0.5f;
            }
            break;
        }
        case FightEvent::Projectile:
            audio_.play(Sfx::Projectile);
            spawnSparks(e.pos, 8, sc, WHITE, 2.0f, 0.1f, 0.4f, Blend::Additive, 0);
            break;
        case FightEvent::Clash:
            audio_.play(Sfx::Clash);
            addBanner("RUSH MODE", Color(1, 0.4f, 0.2f), 2.0f, 6, 0.3f);
            addBanner("MASH ATTACK!", WHITE, 2.5f, 2.8f, 0.42f);
            clashPulse_ = 1.0f;
            break;
        case FightEvent::StyleBreak:
            audio_.play(Sfx::StyleBreak);
            addBanner(e.text, Color(0.6f, 0.4f, 1.0f), 1.5f, 5.5f, 0.35f);
            spawnSparks(e.pos, 30, sc, Color(0.3f, 0.2f, 0.5f), 6.0f, 0.2f, 0.8f, Blend::Additive, 2.0f);
            break;
        case FightEvent::FlashCancel: {
            audio_.play(Sfx::FlashCancel);
            addFloat(e.pos + Vec3(0, 1.2f, 0), "FLASH CANCEL", Color(1, 1, 0.5f), 1.0f, 2.2f);
            Particle flash; flash.pos = e.pos; flash.life = flash.maxLife = 0.25f; flash.size = 2.0f; flash.col = Color(1, 1, 0.7f, 0.9f); flash.growth = 10.0f;
            particles_.push_back(flash);
            spawnSparks(e.pos, 20, Color(1, 1, 0.6f), WHITE, 5.0f, 0.12f, 0.5f, Blend::Additive, 0);
            break;
        }
        case FightEvent::KO:
            audio_.play(Sfx::KO);
            addBanner(e.text, e.text == "DRAMATIC FINISH" ? Color(1, 0.35f, 0.2f) : RED, 2.2f, e.text.size() > 6 ? 5.2f : 8.0f, 0.35f);
            hudFlash_ = 0.85f;
            break;
        case FightEvent::RoundStart:
            audio_.play(Sfx::RoundStart);
            addBanner(e.text, WHITE, 1.5f, 5.5f);
            break;
        case FightEvent::Tech:
            audio_.play(Sfx::Tech);
            addFloat(e.pos, "THROW TECH", Color(0.7f, 0.9f, 1), 1.0f, 2.0f);
            spawnSparks(e.pos, 10, Color(0.7f, 0.9f, 1), WHITE, 3.0f, 0.1f, 0.4f, Blend::Additive, 2.0f);
            break;
        }
    }
    sim_.events().clear();
}

void App::updateEffects(float dt) {
    for (Particle& p : particles_) {
        p.life -= dt;
        p.vel.y -= p.gravity * dt;
        p.pos += p.vel * dt;
        p.size += p.growth * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.life <= 0; }), particles_.end());
    for (FloatText& f : floats_) { f.life -= dt; f.pos.y += dt * 0.6f; }
    floats_.erase(std::remove_if(floats_.begin(), floats_.end(), [](const FloatText& f) { return f.life <= 0; }), floats_.end());
    for (Banner& b : banners_) b.life -= dt;
    banners_.erase(std::remove_if(banners_.begin(), banners_.end(), [](const Banner& b) { return b.life <= 0; }), banners_.end());
    hudFlash_ = std::max(0.0f, hudFlash_ - dt * 2.5f);
    clashPulse_ = std::max(0.0f, clashPulse_ - dt);
}

void App::updateFight(float dt) {
    const PadState& p1 = input_.player(0);
    const PadState& p2 = input_.player(1);
    const PadState& m = input_.menu();
    bool matchOver = sim_.phase() == Phase::MatchOver;

    // Keep live gameplay settings synced
    sim_.setDamageScale(settings_.damageScale());
    sim_.setHitstopEnabled(settings_.hitstopEnabled);
    sim_.setEasyControls(0, settings_.easyControls[0]);
    sim_.setEasyControls(1, settings_.easyControls[1]);

    // Esc / Enter opens the pause menu; once open, Enter confirms and Esc backs out
    if (!matchOver && !paused_ && fightMode_ != FightMode::Demo && (m.justPressed(BTN_BACK) || m.justPressed(BTN_START))) {
        paused_ = true; pauseIndex_ = 0; showMoveList_ = false;
        return;
    }
    if (paused_) {
        if (showMoveList_) {
            if (m.justPressed(BTN_BACK) || m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT) || input_.leftClicked() || input_.rightClicked())
                showMoveList_ = false;
            return;
        }
        int n = 4;
        bool clicked = false;
        for (int i = 0; i < n; ++i) {
            if (!input_.mouseIn(width_ * 0.5f - 180, height_ * 0.45f + i * 44 - 6, 360, 36)) continue;
            if (input_.mouseMoved() || input_.leftClicked()) pauseIndex_ = i;
            clicked = input_.leftClicked();
        }
        if (m.justPressed(BTN_DOWN)) pauseIndex_ = (pauseIndex_ + 1) % n;
        if (m.justPressed(BTN_UP)) pauseIndex_ = (pauseIndex_ + n - 1) % n;
        if (m.justPressed(BTN_BACK) || input_.rightClicked()) { paused_ = false; return; }
        if (m.justPressed(BTN_LIGHT) || m.justPressed(BTN_START) || clicked) {
            if (pauseIndex_ == 0) paused_ = false;
            else if (pauseIndex_ == 1) showMoveList_ = true;
            else if (pauseIndex_ == 2) openSettings(Scene::Fight);
            else { paused_ = false; scene_ = Scene::Title; }
        }
        return;
    }

    // Developer mode frame-pause & single-step
    if (settings_.devMode && devFramePause_) {
        if (devStepOnce_) {
            devStepOnce_ = false;
            SimInput a = p1Ai_ ? ai_[0].think(sim_, 0) : padToSim(p1, 0);
            SimInput b = p2Ai_ ? ai_[1].think(sim_, 1) : padToSim(p2, 1);
            sim_.step(a, b);
            processEvents();
            fightTime_ += 1.0f / 60.0f;
            updateEffects(1.0f / 60.0f);
        }
        return;
    }

    // fixed-step simulation with slow motion on KO (or dev slow-mo)
    float scale = sim_.timeScale() * (settings_.devMode && devSlowMo_ ? 0.25f : 1.0f);
    slowmo_ = lerp(slowmo_, scale, std::min(1.0f, dt * 10.0f));
    simAccum_ += dt * slowmo_;
    const float STEP = 1.0f / 60.0f;
    int steps = 0;
    while (simAccum_ >= STEP && steps < 4) {
        SimInput a = p1Ai_ ? ai_[0].think(sim_, 0) : padToSim(p1, 0);
        SimInput b = p2Ai_ ? ai_[1].think(sim_, 1) : padToSim(p2, 1);
        if (opts_.demo == 6 && sim_.phase() == Phase::Fight) {
            float t = fightTime_;
            a = SimInput(); b = SimInput();
            a.moveF = b.moveF = t < 7.0f ? -1.0f : (t < 9.0f ? 0.0f : 1.0f);
            a.moveS = b.moveS = (t > 4.0f && t < 7.0f) ? 0.6f : 0.0f;
        }
        sim_.step(a, b);
        processEvents();
        simAccum_ -= STEP; steps++;
    }
    if (steps == 4) simAccum_ = 0;
    fightTime_ += dt * slowmo_;
    updateEffects(dt * std::max(slowmo_, 0.4f));

    if (matchOver) {
        resultTimer_ += dt;
        if (fightMode_ == FightMode::Demo) {
            if (resultTimer_ > 3.0f) { int a = fxRng_.range(0, 5), b = fxRng_.range(0, 4); if (b >= a) b++; startFight(a, b, FightMode::Demo, 2, opts_.showcase ? arenaId_ : -1); }
            return;
        }
        if (resultTimer_ < 1.0f) return;
        bool playerWon = sim_.matchWinner() == 0;
        if (fightMode_ == FightMode::Hunt) {
            if (playerWon) {
                hunt_.markDefeated(huntEngaged_);
                huntDefeatedCount_++;
                huntEngaged_ = -1;
                hunt_.resumeAfterFight();
                scene_ = Scene::Hunt;
                huntBannerTimer_ = 0;
                if (hunt_.remaining() == 0) { huntComplete_ = true; huntCompleteTimer_ = 0; input_.setMouseCaptured(false); }
                else input_.setMouseCaptured(true);
                return;
            }
            int n = 2;
            if (m.justPressed(BTN_DOWN) || m.justPressed(BTN_UP)) resultIndex_ = (resultIndex_ + 1) % n;
            bool clicked = false;
            for (int i = 0; i < n; ++i) {
                if (!input_.mouseIn(width_ * 0.5f - 180, height_ * 0.3f + 130 + i * 38 - 6, 360, 32)) continue;
                if (input_.mouseMoved() || input_.leftClicked()) resultIndex_ = i;
                clicked = input_.leftClicked();
            }
            if (m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT) || clicked) {
                if (resultIndex_ == 0) startFight(huntPlayerChar_, hunt_.opponents()[huntEngaged_].charId, FightMode::Hunt, huntDefeatedCount_ < 2 ? 0 : (huntDefeatedCount_ < 4 ? 1 : 2), arenaId_);
                else { scene_ = Scene::Title; }
            }
            return;
        }
        int n = 3;
        if (m.justPressed(BTN_DOWN)) resultIndex_ = (resultIndex_ + 1) % n;
        if (m.justPressed(BTN_UP)) resultIndex_ = (resultIndex_ + n - 1) % n;
        bool clicked = false;
        for (int i = 0; i < n; ++i) {
            if (!input_.mouseIn(width_ * 0.5f - 180, height_ * 0.3f + 100 + i * 38 - 6, 360, 32)) continue;
            if (input_.mouseMoved() || input_.leftClicked()) resultIndex_ = i;
            clicked = input_.leftClicked();
        }
        if (m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT) || clicked) {
            if (resultIndex_ == 0) startFight(sim_.fighter(0).ch - &roster()[0], sim_.fighter(1).ch - &roster()[0], fightMode_, settings_.aiDifficulty, arenaId_);
            else if (resultIndex_ == 1) { scene_ = Scene::Select; selectFor_ = 0; }
            else scene_ = Scene::Title;
        }
    }
}

void App::renderFight() {
    const Fighter& a = sim_.fighter(0);
    const Fighter& b = sim_.fighter(1);
    const Character& ca = *a.ch; const Character& cb = *b.ch;

    // ---- camera (with screen shake scaled by settings)
    updateFightCamera();
    Vec3 shakeOff;
    float sh = sim_.shake() * settings_.shakeScale();
    if (sh > 0) {
        const Vec3 camR(std::cos(fightCamYaw_), 0, -std::sin(fightCamYaw_));
        shakeOff = camR * (fxRng_.uniform(-1, 1) * sh * 0.18f) + Vec3(0, fxRng_.uniform(-1, 1) * sh * 0.14f, 0);
    }
    Camera camA = viewCam_[0], camB = viewCam_[1];
    camA.eye += shakeOff; camA.target += shakeOff; camB.eye += shakeOff; camB.target += shakeOff;
    applyOrbitCamera(camA);

    const Arena& arena = arenas_[arenaId_];
    renderer_.beginFrame(camA, arena.info().clear);
    if (splitActive_ && !orbitCamActive_) renderer_.setSecondView(camB, splitNormal_);
    Vec3 midP = flatV(a.pos + b.pos) * 0.5f;
    float sepP = flatV(b.pos - a.pos).length();
    renderer_.setShadowFocus(midP, std::max(11.0f, sepP * 0.5f + 5.0f));
    arena.render(renderer_, rig_.unitBox(), time_, ca.styleColor, cb.styleColor);
    renderer_.addPointLight(a.pos + Vec3(0, 3.0f, 0), ca.styleColor.rgb() * 0.8f, 6.5f);
    renderer_.addPointLight(b.pos + Vec3(0, 3.0f, 0), cb.styleColor.rgb() * 0.8f, 6.5f);
    renderer_.addPointLight(fightCam_.eye + Vec3(0, 1.5f, 0), Vec3(1.0f, 0.93f, 0.82f) * 0.85f, 16);
    if (hudFlash_ > 0) renderer_.addPointLight(midP + Vec3(0, 2.5f, 0), Vec3(1, 1, 1) * hudFlash_ * 0.5f, 10);

    // ---- fighters with smooth pose blending
    const Fighter* fs[2] = {&a, &b};
    for (int i = 0; i < 2; ++i) {
        const Fighter& f = *fs[i];
        const Character& c = *f.ch;
        displayYaw_[i] += wrapAngle(f.yaw() - displayYaw_[i]) * (1.0f - std::exp(-18.0f * frameDt_));
        float yaw = displayYaw_[i];
        const Vec3 fwd = f.fwd, right = f.right();
        Pose targetPose = poseForFighter(f, fightTime_ + i * 0.37f);
        if (!displayPoseInit_[i]) {
            displayPose_[i] = targetPose;
            displayPoseInit_[i] = true;
        } else {
            float poseRate = (f.state == FState::Attack || f.inHitstun()) ? 32.0f : 22.0f;
            float blendK = 1.0f - std::exp(-poseRate * frameDt_);
            displayPose_[i] = lerpPose(displayPose_[i], targetPose, blendK);
        }
        const Pose& p = displayPose_[i];
        float hs = (float)f.hitstop;
        Vec3 pos = f.pos;
        if (hs > 0 && f.inHitstun()) pos += right * (std::sin(fightTime_ * 90.0f) * 0.03f);
        float flash = f.flash * 0.8f;
        if (f.state == FState::StyleBreak) flash = 0.25f + 0.25f * std::sin(fightTime_ * 20);
        bool modeOn = c.style == StyleType::Mode && f.styleOn;
        bool onFloor = pos.y >= -0.02f;
        rig_.draw(renderer_, p, pos, yaw, c, 1.0f, 1.0f, flash, false, onFloor, ARENA_SURFACE_Y);
        if (modeOn) {
            renderer_.drawBillboard(pos + Vec3(0, 1.0f, 0), 2.5f + 0.18f * std::sin(fightTime_ * 10), c.styleColor.withAlpha(0.20f), Blend::Additive, fightTime_ * 2);
            if (fxRng_.chance(0.35f)) {
                Particle s; s.pos = pos + Vec3(fxRng_.uniform(-0.4f, 0.4f), fxRng_.uniform(0.1f, 1.8f), fxRng_.uniform(-0.3f, 0.3f));
                s.vel = Vec3(0, fxRng_.uniform(0.8f, 2.0f), 0); s.life = s.maxLife = 0.6f; s.size = 0.08f; s.col = c.styleColor.withAlpha(0.8f);
                particles_.push_back(s);
            }
        }
        // Stand
        if (c.style == StyleType::Stand && f.styleOn) {
            float blend = f.standAttackBlend;
            Vec3 behind = pos - fwd * 0.85f - right * 0.4f;
            behind.y = pos.y + 0.3f + 0.08f * std::sin(fightTime_ * 2.5f + i);
            Vec3 front = pos + fwd * 0.95f + Vec3(0, 0.05f, 0);
            Vec3 target = lerp(behind, front, blend);
            if (!standPosInit_) { standPos_[0] = standPos_[1] = target; }
            standPos_[i] = lerp(standPos_[i], target, 1.0f - std::pow(1.0f - 0.35f, frameDt_ * 60.0f));
            Pose sp;
            if (f.state == FState::Attack && f.move && f.move->standAttack) {
                const Move* m = f.move;
                float ph;
                if (f.moveFrame < m->startup) ph = (float)f.moveFrame / (float)std::max(1, m->startup);
                else if (f.moveFrame < m->startup + m->active) ph = 1.0f + (float)(f.moveFrame - m->startup) / (float)std::max(1, m->active);
                else ph = 2.0f + (float)(f.moveFrame - m->startup - m->active) / (float)std::max(1, m->recovery);
                sp = attackPose(m->anim, ph, fightTime_, false);
                sp.hipHeight += 0.1f;
            } else if (f.state == FState::Clash) {
                sp = attackPose(AnimKind::Rush, 1.5f, fightTime_, false); sp.hipHeight += 0.1f;
            } else sp = standIdlePose(fightTime_ + i);
            rig_.draw(renderer_, sp, standPos_[i], yaw, c, 1.08f, 0.55f, 0, true, false);
            renderer_.drawBillboard(standPos_[i] + Vec3(0, 1.3f, 0), 2.3f, c.styleColor.withAlpha(0.15f), Blend::Additive, fightTime_ * 1.5f);
        }
        // blocking shield flash
        if (f.state == FState::Block || f.state == FState::CrouchBlock) {
            float k = 1.0f - (float)f.timer / (float)std::max(1, f.stateLen);
            renderer_.drawBillboard(pos + fwd * 0.5f + Vec3(0, 1.0f, 0), 1.0f, Color(0.5f, 0.75f, 1.0f, 0.35f * k), Blend::Additive, fightTime_ * 4);
        }
        if (f.invulStrikes > 0 && (f.state == FState::Sidestep || f.state == FState::Backdash || f.state == FState::Dash)) {
            renderer_.drawBillboard(pos + Vec3(0, 1.0f, 0), 1.5f, Color(0.8f, 0.9f, 1.0f, 0.2f), Blend::Additive, fightTime_ * 6);
        }
    }
    standPosInit_ = true;

    // ---- projectiles
    for (const Projectile& p : sim_.projectiles()) {
        Mat4 m = Mat4::translate(p.pos) * Mat4::rotateY(p.spin) * Mat4::rotateZ(p.spin * 0.7f) * Mat4::scale(Vec3(0.5f, 0.5f, 0.5f));
        renderer_.draw(projectileMesh_, m, mix(p.color, WHITE, 0.3f), 1.0f, 0, false);
        renderer_.drawBillboard(p.pos, 1.4f, p.color.withAlpha(0.5f), Blend::Additive, p.spin);
        Particle t; t.pos = p.pos + Vec3(fxRng_.uniform(-0.1f, 0.1f), fxRng_.uniform(-0.1f, 0.1f), fxRng_.uniform(-0.1f, 0.1f));
        t.vel = p.dir * (-p.speed * 8.0f) + Vec3(0, fxRng_.uniform(-0.5f, 0.5f), 0); t.life = t.maxLife = 0.35f; t.size = 0.18f; t.col = p.color.withAlpha(0.8f);
        particles_.push_back(t);
    }
    // ---- particles
    for (const Particle& p : particles_) {
        float k = clampf(p.life / p.maxLife, 0, 1);
        Color c = p.col; c.a *= k;
        renderer_.drawBillboard(p.pos, p.size, c, p.blend, p.life * 3.0f);
    }
    // clash lightning between the fighters
    if (sim_.clash().active) {
        Vec3 ap = a.pos + Vec3(0, 1.2f, 0), bp = b.pos + Vec3(0, 1.2f, 0);
        for (int k = 0; k < 4; ++k) {
            Vec3 prev = ap;
            for (int s = 1; s <= 6; ++s) {
                Vec3 pt = lerp(ap, bp, s / 6.0f) + Vec3(0, fxRng_.uniform(-0.4f, 0.4f), fxRng_.uniform(-0.3f, 0.3f));
                if (s == 6) pt = bp;
                renderer_.drawLine3D(prev, pt, 0.05f, (k % 2 ? ca.styleColor : cb.styleColor).withAlpha(0.8f), Blend::Additive);
                prev = pt;
            }
        }
    }

    if (settings_.devMode) {
        drawDevFightWorld();
    }

    drawFightHud();
    drawDevOverlay();
    renderer_.endFrame();
}

void App::drawFightHud() {
    const Fighter& a = sim_.fighter(0);
    const Fighter& b = sim_.fighter(1);
    const Fighter* fs[2] = {&a, &b};
    float W = (float)width_, H = (float)height_;
    float cx = W * 0.5f;

    // floating texts (world space)
    for (const FloatText& f : floats_) {
        float sx, sy;
        if (!renderer_.projectToScreen(f.pos, sx, sy)) continue;
        float k = clampf(f.life / f.maxLife, 0, 1);
        float al = k < 0.3f ? k / 0.3f : 1.0f;
        renderer_.textCentered(sx + 2, sy + 2, f.text, f.scale, Color(0, 0, 0, 0.65f * al));
        renderer_.textCentered(sx, sy, f.text, f.scale, f.col.withAlpha(al));
    }

    // split screen divider fades in as the views separate
    if (splitAmount_ > 0.01f) {
        Vec2 t(-splitNormal_.y, splitNormal_.x);
        float L = W + H;
        float x0 = cx - t.x * L, y0 = H * 0.5f - t.y * L, x1 = cx + t.x * L, y1 = H * 0.5f + t.y * L;
        renderer_.line(x0, y0, x1, y1, 7.0f, Color(0, 0, 0, 0.75f * splitAmount_));
        renderer_.line(x0, y0, x1, y1, 2.5f, mix(a.ch->styleColor, b.ch->styleColor, 0.5f).withAlpha(0.9f * splitAmount_));
    }
    // off-screen opponent indicator arrow
    if (fightMode_ == FightMode::VsCpu || fightMode_ == FightMode::Hunt) {
        float sx, sy;
        bool on = renderer_.projectToScreen(b.pos + Vec3(0, 1.1f, 0), sx, sy);
        const float m = 46.0f;
        if (!on || sx < m || sx > W - m || sy < 130.0f || sy > H - 90.0f) {
            Vec3 d = b.pos - fightCam_.eye;
            Vec3 camF = normalize(fightCam_.target - fightCam_.eye), camRt = normalize(cross(camF, Vec3(0, 1, 0)));
            Vec2 dir(dot(d, camRt), -dot(d, cross(camRt, camF)) * 0.4f);
            float l = dir.length();
            dir = l > 1e-4f ? dir * (1.0f / l) : Vec2(1, 0);
            float ex = clampf(cx + dir.x * W, m, W - m), ey = clampf(H * 0.5f + dir.y * H, 130.0f, H - 90.0f);
            Vec2 nrm(-dir.y, dir.x);
            renderer_.triangle(ex + dir.x * 18, ey + dir.y * 18, ex - dir.x * 10 + nrm.x * 14, ey - dir.y * 10 + nrm.y * 14,
                               ex - dir.x * 10 - nrm.x * 14, ey - dir.y * 10 - nrm.y * 14, b.ch->styleColor.withAlpha(0.9f));
            renderer_.textCentered(ex - dir.x * 34, ey - dir.y * 34 - 8, b.ch->name, 1.6f, b.ch->styleColor);
        }
    }

    // Strict architectural health bars & trait status
    float barY = 24, barH = 22, margin = 38, barW = cx - margin - 68;
    for (int i = 0; i < 2; ++i) {
        const Fighter& f = *fs[i];
        const Character& c = *f.ch;
        float hpFrac = clampf(f.hpShown / c.hp, 0, 1), redFrac = clampf(f.hpRedShown / c.hp, 0, 1);
        float x0 = i == 0 ? margin : cx + 68;
        renderer_.rect(x0 - 12, barY - 10, barW + 24, 96, UI_PANEL);
        renderer_.rectOutline(x0 - 12, barY - 10, barW + 24, 96, 1, UI_BORDER);
        renderer_.rect(x0 - 12, barY - 10, barW + 24, 2, c.styleColor);

        renderer_.rect(x0 - 2, barY - 2, barW + 4, barH + 4, Color(0.02f, 0.022f, 0.03f, 0.9f));
        renderer_.rectOutline(x0 - 2, barY - 2, barW + 4, barH + 4, 1, UI_BORDER);
        renderer_.rect(x0, barY, barW, barH, Color(0.12f, 0.05f, 0.06f));
        float rw = barW * redFrac, hw = barW * hpFrac;
        float rx = i == 0 ? x0 : x0 + barW - rw;
        float hx = i == 0 ? x0 : x0 + barW - hw;
        renderer_.rect(rx, barY, rw, barH, Color(0.72f, 0.16f, 0.14f));
        Color hpCol = hpFrac > 0.5f ? Color(0.88f, 0.76f, 0.34f) : (hpFrac > 0.25f ? Color(0.92f, 0.48f, 0.18f) : Color(0.86f, 0.20f, 0.20f));
        if (f.state == FState::StyleBreak) hpCol = Color(0.58f, 0.42f, 0.92f);
        renderer_.rect(hx, barY, hw, barH, hpCol);
        renderer_.rect(hx, barY, hw, 4, WHITE.withAlpha(0.22f));

        // Fighter name & trait badge
        float ny = barY + barH + 7;
        std::string statusTag = c.traitName;
        Color statusCol = UI_SUBTEXT;
        if (f.burnTimer > 0) { statusTag = "BURNING"; statusCol = Color(1.0f, 0.45f, 0.15f); }
        else if (f.chillTimer > 0) { statusTag = "CHILLED"; statusCol = CYAN; }
        else if (c.trait == TraitKind::TailwindStep && f.galeChargeTimer > 0) { statusTag = "GALE READY"; statusCol = GREEN; }
        else if (c.trait == TraitKind::BloodRush && f.hp < c.hp * 0.4f) { statusTag = "BLOOD RUSH ACTIVE"; statusCol = RED; }

        if (i == 0) {
            renderer_.text(x0, ny, c.name, 2.2f, WHITE);
            renderer_.text(x0 + renderer_.textWidth(c.name, 2.2f) + 14.0f, ny + 4.0f, "[" + statusTag + "]", 1.3f, statusCol);
        } else {
            renderer_.textRight(x0 + barW, ny, c.name, 2.2f, WHITE);
            renderer_.textRight(x0 + barW - renderer_.textWidth(c.name, 2.2f) - 14.0f, ny + 4.0f, "[" + statusTag + "]", 1.3f, statusCol);
        }

        // Style gauge
        float sgW = barW * 0.5f, sgY = ny + 24;
        float sx0 = i == 0 ? x0 : x0 + barW - sgW;
        renderer_.rect(sx0, sgY, sgW, 6, Color(0.08f, 0.09f, 0.12f, 0.9f));
        float sw = sgW * clampf(f.styleGauge, 0, 1);
        renderer_.rect(i == 0 ? sx0 : sx0 + sgW - sw, sgY, sw, 6, f.styleOn ? c.styleColor : c.styleColor * 0.6f);
        std::string sl = c.styleName + (f.styleOn ? "  [ACTIVE]" : "");
        if (i == 0) renderer_.text(sx0, sgY + 9, sl, 1.35f, f.styleOn ? c.styleColor : DIM);
        else renderer_.textRight(sx0 + sgW, sgY + 9, sl, 1.35f, f.styleOn ? c.styleColor : DIM);

        // Round pips
        for (int r = 0; r < sim_.roundsToWin(); ++r) {
            float px = i == 0 ? x0 + barW - 14 - r * 18 : x0 + 2 + r * 18;
            renderer_.rect(px, ny + 4, 11, 11, r < f.roundsWon ? GOLD : Color(0.16f, 0.18f, 0.22f));
            renderer_.rectOutline(px, ny + 4, 11, 11, 1, UI_BORDER);
        }

        // Heart Heat Gauge (bottom)
        float segW = 92, segH = 14, gap = 6, hy = H - 46;
        float totalW = segW * 3 + gap * 2;
        float hx0 = i == 0 ? margin : W - margin - totalW;
        renderer_.rect(hx0 - 10, hy - 26, totalW + 20, 50, UI_PANEL);
        renderer_.rectOutline(hx0 - 10, hy - 26, totalW + 20, 50, 1, UI_BORDER);
        renderer_.rect(hx0 - 10, hy - 26, totalW + 20, 2, c.styleColor);
        int bars = (int)(f.hhg / 1000.0f);
        for (int s = 0; s < 3; ++s) {
            float sx = hx0 + s * (segW + gap);
            renderer_.rect(sx - 1, hy - 1, segW + 2, segH + 2, Color(0.02f, 0.025f, 0.035f, 0.9f));
            renderer_.rect(sx, hy, segW, segH, Color(0.10f, 0.11f, 0.14f));
            float fill = clampf((f.hhg - s * 1000.0f) / 1000.0f, 0, 1);
            Color col = s < bars ? GOLD : Color(0.78f, 0.36f, 0.18f);
            if (f.hhg >= 3000) col = WHITE;
            renderer_.rect(sx, hy, segW * fill, segH, col);
        }
        std::string hl = "HEAT GAUGE  " + std::to_string(bars) + " / 3";
        if (i == 0) renderer_.text(hx0, hy - 20, hl, 1.5f, GOLD);
        else renderer_.textRight(hx0 + totalW, hy - 20, hl, 1.5f, GOLD);

        // Combo counter
        if (f.comboShow > 0 && f.comboHits >= 2) {
            float k = clampf(f.comboShow / 30.0f, 0, 1);
            float cxp = i == 0 ? margin + 10 : W - margin - 10;
            std::string hits = std::to_string(f.comboHits) + " HITS";
            std::string dmg = std::to_string((int)f.comboDamage) + " DAMAGE";
            Color cc = f.comboHits >= 8 ? RED : (f.comboHits >= 5 ? GOLD : WHITE);
            if (i == 0) {
                renderer_.text(cxp, H * 0.33f, hits, 3.8f, cc.withAlpha(k));
                renderer_.text(cxp, H * 0.33f + 34, dmg, 1.8f, UI_SUBTEXT.withAlpha(k));
            } else {
                renderer_.textRight(cxp, H * 0.33f, hits, 3.8f, cc.withAlpha(k));
                renderer_.textRight(cxp, H * 0.33f + 34, dmg, 1.8f, UI_SUBTEXT.withAlpha(k));
            }
        }
        if (f.lastMoveNameTimer > 0 && f.lastMoveNameTimer < 70) {
            float k = clampf(f.lastMoveNameTimer / 20.0f, 0, 1);
            if (i == 0) renderer_.text(margin, H * 0.33f + 66, displayName(f.lastMoveName), 1.8f, f.ch->styleColor.withAlpha(k));
            else renderer_.textRight(W - margin, H * 0.33f + 66, displayName(f.lastMoveName), 1.8f, f.ch->styleColor.withAlpha(k));
        }
    }

    // Center Timer Box
    renderer_.rect(cx - 52, 14, 104, 54, UI_PANEL);
    renderer_.rectOutline(cx - 52, 14, 104, 54, 1.5f, GOLD);
    int secs = sim_.secondsLeft();
    char buf[16];
    if (secs >= 999) std::snprintf(buf, sizeof(buf), "INF");
    else std::snprintf(buf, sizeof(buf), "%02d", secs);
    renderer_.textCentered(cx, 23, buf, 3.6f, (secs <= 10 && secs < 999) ? RED : WHITE);
    renderer_.textCentered(cx, 72, "ROUND " + std::to_string(sim_.round()), 1.45f, UI_SUBTEXT);

    if (settings_.showHudPrompts && fightMode_ != FightMode::Demo && !paused_) {
        renderer_.textCentered(cx, H - 28, "ESC / ENTER  PAUSE & COMMAND LIST", 1.3f, DIM);
    }

    // rush clash meter
    if (sim_.clash().active) {
        const ClashState& c = sim_.clash();
        float total = (float)std::max(1, c.presses[0] + c.presses[1]);
        float frac = c.presses[0] / total;
        float mw = 420, mh = 20, mx = cx - mw * 0.5f, my = H * 0.52f;
        renderer_.rect(mx - 3, my - 3, mw + 6, mh + 6, UI_PANEL);
        renderer_.rectOutline(mx - 3, my - 3, mw + 6, mh + 6, 1, GOLD);
        renderer_.rect(mx, my, mw * frac, mh, a.ch->styleColor);
        renderer_.rect(mx + mw * frac, my, mw * (1 - frac), mh, b.ch->styleColor);
        float timeLeft = (150 - c.timer) / 60.0f;
        std::snprintf(buf, sizeof(buf), "%.1f", timeLeft);
        renderer_.textCentered(cx, my + 28, buf, 1.9f, WHITE);
    }

    // banners
    for (const Banner& bn : banners_) {
        float k = clampf(bn.life / bn.maxLife, 0, 1);
        float al = k < 0.25f ? k / 0.25f : 1.0f;
        float pop = 1.0f + 0.18f * std::max(0.0f, (k - 0.9f) / 0.1f);
        float y = H * bn.yFrac;
        renderer_.textCentered(cx + 3, y + 3, bn.text, bn.scale * pop, Color(0, 0, 0, 0.7f * al));
        renderer_.textCentered(cx, y, bn.text, bn.scale * pop, bn.col.withAlpha(al));
    }
    if (hudFlash_ > 0) renderer_.rect(0, 0, W, H, Color(1, 1, 1, 0.08f * hudFlash_));

    // intro: show style names & traits
    if (sim_.phase() == Phase::Intro && sim_.phaseTimer() < 110) {
        float al = clampf((110 - sim_.phaseTimer()) / 25.0f, 0, 1);
        renderer_.text(margin, H * 0.6f, a.ch->name, 3.8f, WHITE.withAlpha(al));
        renderer_.text(margin, H * 0.6f + 38, a.ch->styleName + "  |  " + a.ch->traitName, 1.7f, a.ch->styleColor.withAlpha(al));
        renderer_.textRight(W - margin, H * 0.6f, b.ch->name, 3.8f, WHITE.withAlpha(al));
        renderer_.textRight(W - margin, H * 0.6f + 38, b.ch->styleName + "  |  " + b.ch->traitName, 1.7f, b.ch->styleColor.withAlpha(al));
        renderer_.textCentered(cx, H * 0.6f + 16, "VS", 4.4f, GOLD.withAlpha(al));
    }

    // match over
    if (sim_.phase() == Phase::MatchOver && resultTimer_ > 0.8f) {
        float al = clampf((resultTimer_ - 0.8f) * 2.0f, 0, 1);
        renderer_.rect(cx - 260, H * 0.28f, 520, 240, UI_PANEL.withAlpha(0.94f * al));
        renderer_.rectOutline(cx - 260, H * 0.28f, 520, 240, 1, UI_BORDER.withAlpha(al));
        renderer_.rect(cx - 260, H * 0.28f, 520, 3, GOLD.withAlpha(al));
        int w = sim_.matchWinner();
        const Character& wc = *sim_.fighter(w).ch;
        renderer_.textCentered(cx, H * 0.3f + 16, wc.name + " WINS", 4.8f, wc.styleColor.withAlpha(al));
        if (fightMode_ == FightMode::Hunt) {
            if (w == 0) renderer_.textCentered(cx, H * 0.3f + 90, "RETURNING TO THE HALLS...", 2.2f, WHITE.withAlpha(al));
            else {
                renderer_.textCentered(cx, H * 0.3f + 78, "YOU WERE FOUND WANTING", 2.1f, RED.withAlpha(al));
                const char* items[2] = {"RETRY THE FIGHT", "GIVE UP"};
                for (int i = 0; i < 2; ++i) {
                    bool sel = (i == resultIndex_);
                    renderer_.textCentered(cx, H * 0.3f + 130 + i * 38, items[i], 2.3f, (sel ? GOLD : WHITE).withAlpha(al));
                }
            }
        } else if (fightMode_ != FightMode::Demo) {
            const char* items[3] = {"REMATCH", "CHARACTER SELECT", "TITLE"};
            for (int i = 0; i < 3; ++i) {
                bool sel = (i == resultIndex_);
                renderer_.textCentered(cx, H * 0.3f + 100 + i * 38, items[i], 2.3f, (sel ? GOLD : WHITE).withAlpha(al));
            }
        }
    }
    if (paused_) { if (showMoveList_) drawMoveList(); else drawPauseMenu(); }
}

void App::drawPauseMenu() {
    float W = (float)width_, H = (float)height_, cx = W * 0.5f;
    renderer_.rect(0, 0, W, H, Color(0.01f, 0.012f, 0.018f, 0.78f));
    renderer_.rect(cx - 230, H * 0.26f, 460, 340, UI_PANEL);
    renderer_.rectOutline(cx - 230, H * 0.26f, 460, 340, 1, UI_BORDER);
    renderer_.rect(cx - 230, H * 0.26f, 460, 3, GOLD);
    renderer_.textCentered(cx, H * 0.31f, "MATCH PAUSED", 3.6f, WHITE);
    const char* items[4] = {"RESUME", "MOVE LIST", "SETTINGS", "QUIT TO TITLE"};
    for (int i = 0; i < 4; ++i) {
        float y = H * 0.45f + i * 44;
        bool sel = (i == pauseIndex_);
        renderer_.rect(cx - 180, y - 6, 360, 36, sel ? UI_PANEL_HI : Color(0.05f, 0.055f, 0.075f, 0.75f));
        renderer_.rectOutline(cx - 180, y - 6, 360, 36, 1, sel ? GOLD : UI_BORDER);
        if (sel) renderer_.rect(cx - 180, y - 6, 3, 36, GOLD);
        renderer_.textCentered(cx, y + 3, items[i], 2.2f, sel ? GOLD : WHITE);
    }
    renderer_.textCentered(cx, H * 0.85f, "UP / DOWN / MOUSE  CHOOSE     ENTER / CLICK  CONFIRM     ESC  RESUME", 1.45f, DIM);
}

void App::drawMoveList() {
    float W = (float)width_, H = (float)height_;
    renderer_.rect(0, 0, W, H, Color(0.01f, 0.012f, 0.018f, 0.86f));
    renderer_.rect(44, 28, W - 88, H - 56, UI_PANEL);
    renderer_.rectOutline(44, 28, W - 88, H - 56, 1, UI_BORDER);
    renderer_.rect(44, 28, W - 88, 3, GOLD);
    const Character& c = *sim_.fighter(0).ch;
    float x = 72, y = 48;
    renderer_.text(x, y, c.name + "  -  " + c.styleName + "  (" + c.traitName + ")", 2.6f, c.styleColor);
    std::vector<std::string> lines = {
        "TRAIT: " + c.traitDesc,
        "",
        "MOVE FREELY  WASD     JUMP  SPACE     CROUCH  CTRL     W / S  SIDESTEP",
        "8-WAY OMNIDIRECTIONAL DASH:  DIRECTION + DASH (U)   OR   DOUBLE-TAP ANY DIRECTION",
        "LIGHT J   MEDIUM K   HEAVY L     HOLD BACK TO BLOCK",
        "EASY BEAT AUTO-COMBO: TAP LIGHT REPEATEDLY  (CAN BE TOGGLED PER PLAYER IN SETTINGS)",
        "F + LIGHT / MEDIUM: FORWARD SPECIAL     B + LIGHT / MEDIUM: BACK SPECIAL     F + HEAVY: RISING SPECIAL",
        "HOLD F + J+K: HHA (1 BAR)     HOLD F + J+K+L: GHA (2 BARS)",
        "THROW  J+K     STYLE / STAND  I     TAUNT  O     FLASH CANCEL  J+K+L DURING ATTACK",
        "",
        "SIGNATURE TECHNIQUES",
    };
    for (const Move& m : c.moves) {
        if (m.kind == MoveKind::Special && m.buttons == BTN_LIGHT) {
            std::string mo = m.motion == Motion::QCF ? "F + LIGHT" : m.motion == Motion::QCB ? "B + LIGHT" : "F + HEAVY";
            std::string tag = m.projectile ? "  [PROJECTILE]" : m.commandThrow ? "  [COMMAND THROW]" : m.rush ? "  [RUSH]" : m.armor ? "  [ARMOR]" : "";
            lines.push_back("  " + upper(m.name.substr(0, m.name.size() - 2)) + "   " + mo + tag);
        }
    }
    for (const Move& m : c.moves) if (m.kind == MoveKind::HHA) lines.push_back("  HHA: " + m.name);
    for (const Move& m : c.moves) if (m.kind == MoveKind::GHA) lines.push_back("  GHA: " + m.name);
    drawPanelText(x, y + 44, lines, 1.55f, WHITE, 1.5f);
    renderer_.textCentered(W * 0.5f, H - 52, "ENTER / ESC / CLICK  RETURN", 1.5f, DIM);
}

// ------------------------------------------------------------------ dev mode --
void App::applyOrbitCamera(Camera& cam) const {
    if (!orbitCamActive_) return;
    float cp = std::cos(orbitPitch_);
    Vec3 offset(std::sin(orbitYaw_) * cp * orbitDist_,
                std::sin(orbitPitch_) * orbitDist_,
                std::cos(orbitYaw_) * cp * orbitDist_);
    cam.eye = cam.target + offset;
}

void App::updateDevKeys() {
    if (scene_ == Scene::Settings && settingsCapturing_) return;

    auto keyEdge = [&](int glfwKey, int slot) {
        bool down = glfwGetKey(window_, glfwKey) == GLFW_PRESS;
        bool edge = down && !devKeysWas_[slot];
        devKeysWas_[slot] = down;
        return edge;
    };

    // AITU Defence Global Shortcuts:
    // F1 or H: Toggle Help & Graphics Pipeline overlay
    if (keyEdge(GLFW_KEY_F1, 7) || keyEdge(GLFW_KEY_H, 8)) {
        showHelp_ = !showHelp_;
    }
    // F2: Cycle Headline Shader Pipeline Mode (Full -> Direct -> NoBloom -> NoOutline -> NoShadows -> NoSsao -> NoPbr -> NoTextures)
    if (keyEdge(GLFW_KEY_F2, 9)) {
        settings_.renderMode = (settings_.renderMode + 1) % RENDER_MODE_COUNT;
        renderer_.setDebugMode(static_cast<RenderDebugMode>(settings_.renderMode));
    }
    // P: Pause / Resume 3D Animation & Physics Time
    if (keyEdge(GLFW_KEY_P, 10)) {
        animPaused_ = !animPaused_;
    }
    // V: Toggle Interactive 3D Orbit Camera
    if (keyEdge(GLFW_KEY_V, 11)) {
        orbitCamActive_ = !orbitCamActive_;
        if (orbitCamActive_) {
            Vec3 d = renderer_.camera().eye - renderer_.camera().target;
            float len = d.length();
            if (len > 0.5f) {
                orbitDist_ = clampf(len, 2.0f, 35.0f);
                orbitYaw_ = std::atan2(d.x, d.z);
                orbitPitch_ = clampf(std::asin(clampf(d.y / len, -0.95f, 0.95f)), -0.25f, 1.35f);
            }
        }
    }
    // F10: Live Hot-Reload GLSL Shaders from shaders/ directory
    if (keyEdge(GLFW_KEY_F10, 12)) {
        if (renderer_.reloadShaders()) {
            shaderReloadToast_ = 3.0f;
        }
    }
    // 0: Reset Directional Light & Orbit Camera
    if (keyEdge(GLFW_KEY_0, 13)) {
        renderer_.resetLight();
    }

    // Continuous Directional Light Rotation ([ / ] azimuth, - / = elevation)
    if (glfwGetKey(window_, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        renderer_.rotateLight(-70.0f * frameDt_, 0.0f);
    if (glfwGetKey(window_, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        renderer_.rotateLight(70.0f * frameDt_, 0.0f);
    if (glfwGetKey(window_, GLFW_KEY_MINUS) == GLFW_PRESS) {
        if (orbitCamActive_) orbitDist_ = clampf(orbitDist_ + 8.0f * frameDt_, 1.8f, 40.0f);
        else renderer_.rotateLight(0.0f, -42.0f * frameDt_);
    }
    if (glfwGetKey(window_, GLFW_KEY_EQUAL) == GLFW_PRESS) {
        if (orbitCamActive_) orbitDist_ = clampf(orbitDist_ - 8.0f * frameDt_, 1.8f, 40.0f);
        else renderer_.rotateLight(0.0f, 42.0f * frameDt_);
    }

    // Interactive 3D Orbit Camera Controls when active (Mouse drag or Arrows/PageUp/PageDown)
    static float prevMx = 0.0f, prevMy = 0.0f;
    static bool dragging = false;
    if (orbitCamActive_) {
        bool btn = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
                   glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        if (btn) {
            if (dragging) {
                orbitYaw_ -= (input_.mouseX() - prevMx) * 0.007f;
                orbitPitch_ = clampf(orbitPitch_ + (input_.mouseY() - prevMy) * 0.006f, -0.20f, 1.42f);
            }
            prevMx = input_.mouseX();
            prevMy = input_.mouseY();
            dragging = true;
        } else {
            dragging = false;
        }
        if (glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS) orbitYaw_ -= 1.6f * frameDt_;
        if (glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS) orbitYaw_ += 1.6f * frameDt_;
        if (glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS) orbitPitch_ = clampf(orbitPitch_ + 1.1f * frameDt_, -0.20f, 1.42f);
        if (glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS) orbitPitch_ = clampf(orbitPitch_ - 1.1f * frameDt_, -0.20f, 1.42f);
        if (glfwGetKey(window_, GLFW_KEY_PAGE_UP) == GLFW_PRESS) orbitDist_ = clampf(orbitDist_ - 8.0f * frameDt_, 1.8f, 40.0f);
        if (glfwGetKey(window_, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) orbitDist_ = clampf(orbitDist_ + 8.0f * frameDt_, 1.8f, 40.0f);
    } else {
        dragging = false;
    }

    if (keyEdge(GLFW_KEY_F3, 0)) {
        settings_.devMode = !settings_.devMode;
        if (!settings_.devMode) {
            devFramePause_ = false;
            devSlowMo_ = false;
        }
    }
    if (!settings_.devMode) return;

    if (keyEdge(GLFW_KEY_F4, 1) && scene_ == Scene::Fight) {
        for (int i = 0; i < 2; ++i) {
            Fighter& f = sim_.fighter(i);
            f.hp = f.hpShown = f.hpRedShown = f.ch->hp;
            f.hhg = 3000.0f;
            f.styleGauge = 1.0f;
            f.burnTimer = 0;
            f.chillTimer = 0;
            if (f.pos.y < 0.0f) { f.pos.y = 0.0f; f.vel = Vec3(); f.state = FState::Idle; }
        }
    }
    if (keyEdge(GLFW_KEY_F5, 2) && scene_ == Scene::Fight) {
        devFramePause_ = !devFramePause_;
    }
    if (keyEdge(GLFW_KEY_F6, 3) && scene_ == Scene::Fight) {
        devFramePause_ = true;
        devStepOnce_ = true;
    }
    if (keyEdge(GLFW_KEY_F7, 4) && scene_ == Scene::Fight) {
        devSlowMo_ = !devSlowMo_;
    }
    if (keyEdge(GLFW_KEY_F8, 5) && scene_ == Scene::Fight) {
        p2Ai_ = !p2Ai_;
    }
    if (keyEdge(GLFW_KEY_F9, 6) && scene_ == Scene::Fight) {
        arenaId_ = (arenaId_ + 1) % ARENA_COUNT;
        sim_.setArena(arenaInfo(arenaId_).shape);
    }
}

void App::drawDevFightWorld() {
    const ArenaShape& ar = sim_.arena();
    // Draw arena boundary ring
    const int segs = 48;
    Color ringCol = ar.hasLavaPit ? Color(1.0f, 0.25f, 0.15f, 0.9f) : Color(0.88f, 0.74f, 0.42f, 0.7f);
    for (int i = 0; i < segs; ++i) {
        float a0 = (2.0f * PI * i) / segs;
        float a1 = (2.0f * PI * (i + 1)) / segs;
        Vec3 p0(std::cos(a0) * ar.radius, 0.05f, std::sin(a0) * ar.radius);
        Vec3 p1(std::cos(a1) * ar.radius, 0.05f, std::sin(a1) * ar.radius);
        renderer_.drawLine3D(p0, p1, 0.03f, ringCol, Blend::Alpha);
    }

    // Draw fighter hurtboxes and active hitboxes
    for (int i = 0; i < 2; ++i) {
        const Fighter& f = sim_.fighter(i);
        float r = FightSim::FIGHTER_RADIUS;
        float y0 = f.pos.y;
        float hurtH = (f.state == FState::Crouch || f.state == FState::CrouchBlock || f.state == FState::CrouchHit) ? 1.2f
                    : (f.isDown() ? 0.5f : 1.8f);
        float y1 = f.pos.y + hurtH;
        Color hCol = (f.invulStrikes > 0 || f.invulThrows > 0) ? GOLD : (i == 0 ? GREEN : CYAN);

        // Top & bottom hurtbox rings + 4 vertical struts
        const int cSegs = 16;
        for (int s = 0; s < cSegs; ++s) {
            float a0 = (2.0f * PI * s) / cSegs;
            float a1 = (2.0f * PI * (s + 1)) / cSegs;
            Vec3 b0(f.pos.x + std::cos(a0) * r, y0, f.pos.z + std::sin(a0) * r);
            Vec3 b1(f.pos.x + std::cos(a1) * r, y0, f.pos.z + std::sin(a1) * r);
            Vec3 t0(f.pos.x + std::cos(a0) * r, y1, f.pos.z + std::sin(a0) * r);
            Vec3 t1(f.pos.x + std::cos(a1) * r, y1, f.pos.z + std::sin(a1) * r);
            renderer_.drawLine3D(b0, b1, 0.018f, hCol, Blend::Alpha);
            renderer_.drawLine3D(t0, t1, 0.018f, hCol, Blend::Alpha);
            if (s % 4 == 0) renderer_.drawLine3D(b0, t0, 0.018f, hCol, Blend::Alpha);
        }

        // Facing vector arrow
        renderer_.drawLine3D(f.pos + Vec3(0, 0.1f, 0), f.pos + f.fwd * 0.9f + Vec3(0, 0.1f, 0), 0.025f, f.ch->styleColor, Blend::Alpha);

        // Active attack hitbox wireframe
        if (f.state == FState::Attack && f.move) {
            const Move* m = f.move;
            for (const HitWindow& hw : m->hits) {
                if (f.moveFrame >= hw.start && f.moveFrame <= hw.end) {
                    Vec3 hc = f.pos + f.fwd * hw.boxPos.x + Vec3(0, hw.boxPos.y, 0) + f.right() * hw.boxPos.z;
                    float hr = std::max(hw.boxSize.x, hw.boxSize.y) * 0.5f;
                    Color atkCol(1.0f, 0.18f, 0.22f, 0.95f);
                    for (int s = 0; s < 16; ++s) {
                        float a0 = (2.0f * PI * s) / 16.0f;
                        float a1 = (2.0f * PI * (s + 1)) / 16.0f;
                        renderer_.drawLine3D(hc + Vec3(std::cos(a0) * hr, std::sin(a0) * hr, 0),
                                             hc + Vec3(std::cos(a1) * hr, std::sin(a1) * hr, 0), 0.024f, atkCol, Blend::Additive);
                        renderer_.drawLine3D(hc + Vec3(std::cos(a0) * hr, 0, std::sin(a0) * hr),
                                             hc + Vec3(std::cos(a1) * hr, 0, std::sin(a1) * hr), 0.024f, atkCol, Blend::Additive);
                    }
                }
            }
        }
    }

    // Projectile hitboxes
    for (const Projectile& p : sim_.projectiles()) {
        float pr = p.hit.boxSize.x * 0.5f;
        for (int s = 0; s < 12; ++s) {
            float a0 = (2.0f * PI * s) / 12.0f;
            float a1 = (2.0f * PI * (s + 1)) / 12.0f;
            renderer_.drawLine3D(p.pos + Vec3(std::cos(a0) * pr, std::sin(a0) * pr, 0),
                                 p.pos + Vec3(std::cos(a1) * pr, std::sin(a1) * pr, 0), 0.02f, GOLD, Blend::Additive);
        }
    }
}

void App::drawDevOverlay() {
    drawShowcaseCaption();
    static const char* PIPE_NAMES[RENDER_MODE_COUNT] = {
        "FULL (every stage on)",
        "DIRECT (forward pass only, no post-processing)",
        "NO BLOOM",
        "NO DEPTH OUTLINE",
        "NO PCF SHADOWS",
        "NO SSAO",
        "BLINN-PHONG (Cook-Torrance off)",
        "NO TEXTURES (normal maps off)"
    };
    static const char* PIPE_SHORT[RENDER_MODE_COUNT] = {
        "FULL PIPELINE", "DIRECT PASS", "NO BLOOM", "NO OUTLINE", "NO SHADOWS", "NO SSAO", "BLINN-PHONG", "NO TEXTURES"
    };
    int rm = std::clamp(settings_.renderMode, 0, RENDER_MODE_COUNT - 1);

    // Compact AITU status bar on Title screen (top-right) or active non-default states in any scene
    if (scene_ == Scene::Title) {
        float bw = 530.0f, bh = 54.0f;
        float bx = width_ - bw - 28.0f, by = 32.0f;
        renderer_.rect(bx, by, bw, bh, Color(0.045f, 0.050f, 0.065f, 0.90f));
        renderer_.rectOutline(bx, by, bw, bh, 1.0f, Color(0.24f, 0.26f, 0.32f, 0.90f));
        renderer_.rect(bx, by, 3.0f, bh, GOLD);
        renderer_.text(bx + 14.0f, by + 8.0f,
                       "TEAM: Ali Yerzhanuly, Daulet Ulgaisyn, Alikhan Kokey  |  Group: IT-2502", 1.25f, WHITE);
        char sline[180];
        std::snprintf(sline, sizeof(sline),
                      "PIPELINE [F2]: %s   |   CAM [V]: %s   |   LIGHT [[/]]: %.0f DEG   |   HELP [F1/H]",
                      PIPE_SHORT[rm], orbitCamActive_ ? "3D ORBIT" : "GAME",
                      renderer_.lightYawDeg());
        renderer_.text(bx + 14.0f, by + 30.0f, sline, 1.15f, GOLD);
    } else if (rm != 0 || orbitCamActive_ || animPaused_ || std::fabs(renderer_.lightYawDeg()) > 0.5f || std::fabs(renderer_.lightPitchDeg()) > 0.5f) {
        char sline[200];
        std::snprintf(sline, sizeof(sline),
                      "PIPE [F2]: %s  |  CAM [V]: %s  |  LIGHT [[/]]: %.0f/%.0f DEG  |  TIME [P]: %s  |  [F1/H] HELP",
                      PIPE_SHORT[rm], orbitCamActive_ ? "3D ORBIT" : "GAME",
                      renderer_.lightYawDeg(), renderer_.lightPitchDeg(),
                      animPaused_ ? "FROZEN" : "LIVE");
        float tw = renderer_.textWidth(sline, 1.2f) + 28.0f;
        float bx = (width_ - tw) * 0.5f, by = height_ - 28.0f;
        renderer_.rect(bx, by, tw, 22.0f, Color(0.03f, 0.035f, 0.048f, 0.92f));
        renderer_.rectOutline(bx, by, tw, 22.0f, 1.0f, GOLD.withAlpha(0.75f));
        renderer_.textCentered(width_ * 0.5f, by + 4.0f, sline, 1.2f, GOLD);
    }

    if (shaderReloadToast_ > 0.0f) {
        float a = std::min(1.0f, shaderReloadToast_);
        float tw = 520.0f, bx = (width_ - tw) * 0.5f, by = 18.0f;
        renderer_.rect(bx, by, tw, 30.0f, Color(0.04f, 0.08f, 0.06f, 0.94f * a));
        renderer_.rectOutline(bx, by, tw, 30.0f, 1.0f, GREEN.withAlpha(a));
        renderer_.textCentered(width_ * 0.5f, by + 7.0f,
                               "GLSL SHADERS HOT-RELOADED FROM shaders/*.vert & shaders/*.frag [F10]", 1.3f, GREEN.withAlpha(a));
    }

    if (settings_.devMode) {
        const char* sceneStr = "TITLE";
        if (scene_ == Scene::Select) sceneStr = "SELECT";
        else if (scene_ == Scene::StageSelect) sceneStr = "STAGE_SELECT";
        else if (scene_ == Scene::Hunt) sceneStr = "HUNT";
        else if (scene_ == Scene::Fight) sceneStr = "FIGHT";
        else if (scene_ == Scene::Settings) sceneStr = "SETTINGS";

        float px = 16.0f;
        float py = (scene_ == Scene::Fight) ? 132.0f : 16.0f;
        float pw = (scene_ == Scene::Fight) ? 495.0f : 360.0f;
        float ph = (scene_ == Scene::Fight) ? 196.0f : 62.0f;

        renderer_.rect(px, py, pw, ph, Color(0.02f, 0.025f, 0.035f, 0.88f));
        renderer_.rectOutline(px, py, pw, ph, 1.0f, CYAN.withAlpha(0.8f));
        renderer_.rect(px, py, 3.0f, ph, CYAN);

        char line1[128];
        std::snprintf(line1, sizeof(line1), "DEV MODE [F3]  |  %.0f FPS (%.2f MS)  |  SCENE: %s",
                      fpsSmoothed_, frameDt_ * 1000.0f, sceneStr);
        renderer_.text(px + 12.0f, py + 8.0f, line1, 1.3f, CYAN);

        if (scene_ != Scene::Fight) {
            char line2[160];
            std::snprintf(line2, sizeof(line2), "PIPE [F2]: %s   WINDOW: %dx%d   VSYNC: %s",
                          PIPE_SHORT[rm], width_, height_, settings_.vsync ? "ON" : "OFF");
            renderer_.text(px + 12.0f, py + 30.0f, line2, 1.25f, UI_SUBTEXT);
        } else {
            const ArenaInfo& ai = arenaInfo(arenaId_);
            char lineArena[160];
            std::snprintf(lineArena, sizeof(lineArena), "ARENA: %s (R=%.1f%s)  %s%s",
                          ai.name, ai.shape.radius, ai.shape.hasLavaPit ? " LAVA-CLIFF" : "",
                          devFramePause_ ? "[PAUSED]" : "", devSlowMo_ ? "[0.25X]" : "");
            renderer_.text(px + 12.0f, py + 28.0f, lineArena, 1.25f, GOLD);

            for (int i = 0; i < 2; ++i) {
                const Fighter& f = sim_.fighter(i);
                float fy = py + 52.0f + i * 54.0f;
                char fbuf1[180];
                std::snprintf(fbuf1, sizeof(fbuf1), "P%d %s (%s): POS(%.2f, %.2f, %.2f) VEL(%.2f, %.2f) STATE:%s",
                              i + 1, f.ch->name.c_str(), (i == 0 ? (p1Ai_ ? "AI" : "HUM") : (p2Ai_ ? "AI" : "HUM")),
                              f.pos.x, f.pos.y, f.pos.z, f.moveF_smooth, f.moveS_smooth, stateName(f.state));
                renderer_.text(px + 12.0f, fy, fbuf1, 1.2f, i == 0 ? GREEN : CYAN);

                char fbuf2[180];
                std::string mv = (f.state == FState::Attack && f.move)
                    ? (f.move->name + " [" + std::to_string(f.moveFrame) + "/" + std::to_string(f.move->totalFrames()) + "F]")
                    : "NONE";
                std::snprintf(fbuf2, sizeof(fbuf2), "   HP:%.0f/%.0f  HEAT:%.0f  STYLE:%.0f%%  MOVE:%s  STUN:%d",
                              f.hp, f.ch->hp, f.hhg, f.styleGauge * 100.0f, mv.c_str(), f.timer);
                renderer_.text(px + 12.0f, fy + 18.0f, fbuf2, 1.2f, WHITE);
            }

            renderer_.text(px + 12.0f, py + ph - 22.0f,
                           "F4:REFILL   F5:PAUSE   F6:STEP   F7:SLOW   F8:P2 AI   F9:STAGE", 1.2f, UI_SUBTEXT);
        }
    }

    // AITU Help & Graphics Pipeline Modal Overlay [F1 / H]
    if (showHelp_) {
        renderer_.rect(0, 0, (float)width_, (float)height_, Color(0.01f, 0.014f, 0.022f, 0.84f));
        float mw = std::min(920.0f, width_ - 64.0f);
        float mh = std::min(620.0f, height_ - 56.0f);
        float mx = (width_ - mw) * 0.5f;
        float my = (height_ - mh) * 0.5f;

        renderer_.rect(mx, my, mw, mh, Color(0.045f, 0.050f, 0.068f, 0.97f));
        renderer_.rect(mx, my, mw, 68.0f, Color(0.075f, 0.082f, 0.108f, 0.98f));
        renderer_.rectOutline(mx, my, mw, mh, 1.5f, Color(0.26f, 0.29f, 0.36f, 0.95f));
        renderer_.rect(mx, my, mw, 3.0f, GOLD);

        renderer_.text(mx + 26.0f, my + 14.0f,
                       "DEATH COMBAT  -  COMPUTER GRAPHICS PIPELINE & CONTROLS", 2.3f, WHITE);
        renderer_.text(mx + 26.0f, my + 42.0f,
                       "AUTHORS: Ali Yerzhanuly, Daulet Ulgaisyn, Alikhan Kokey   |   Astana IT University  -  Group: IT-2502",
                       1.35f, GOLD);

        float col1X = mx + 26.0f;
        float col2X = mx + mw * 0.52f;
        float y = my + 84.0f;

        renderer_.text(col1X, y, "OPENGL 3.3 CORE SHADER PIPELINE (shaders/*.vert, *.frag)", 1.45f, GOLD);
        std::vector<std::string> pipeLines = {
            "1. SHADOW PASS (depth.vert / depth.frag):",
            "   2048x2048 orthographic light-space depth buffer + 3x3 PCF",
            "   soft shadow filtering with slope-scaled bias.",
            "2. FORWARD LIT PASS (lit.vert / lit.frag):",
            "   Cook-Torrance GGX specular (F2: BLINN-PHONG) + wrap diffuse,",
            "   up to 16 point lights, normal-mapped tile / brick / metal",
            "   textures, Fresnel rim, exp-squared fog.",
            "3. DYNAMIC VORONOI SPLIT-SCREEN (mask.frag):",
            "   Hardware stencil buffer (GL_STENCIL_TEST) clips along the",
            "   screen-space perpendicular bisector between P1 and P2.",
            "4. HDR MSAA POST-FX (bright.frag, blur.frag, composite.frag):",
            "   4x MSAA RGBA16F -> half-res SSAO (ssao.frag) -> bloom ->",
            "   depth-edge ink outline -> soft-knee tone map.",
            "5. PROCEDURAL RIG & SDF TEXT (rig.cpp, ui.vert / ui.frag):",
            "   box-and-joint hierarchy posed by angles, distance-field text."
        };
        drawPanelText(col1X, y + 26.0f, pipeLines, 1.24f, WHITE, 1.36f);

        renderer_.text(col2X, y, "INTERACTIVE DEFENCE SHORTCUTS (LIVE DEMO)", 1.45f, GOLD);
        std::vector<std::string> keyLines = {
            "[F1] or [H]    Toggle this Help & Pipeline Reference",
            "[F2]           Cycle Shader Pipeline Mode (Live Comparison)",
            "[P]            Freeze / Resume 3D Animation & Physics Time",
            "[V]            Toggle 3D Free-Orbit Camera (Mouse Drag / Arrows)",
            "[ [ ] / [ ] ]  Rotate Directional Sun/Moon Light Azimuth (Yaw)",
            "[-] / [=]      Tilt Sun Light Elevation (or Zoom in Orbit Cam)",
            "[0]            Reset Directional Light Angle to Default",
            "[F10]          Hot-Reload GLSL Shaders from shaders/ on Disk",
            "[F3]           Toggle Developer Mode Telemetry & 3D Hitboxes",
            "[F4 - F9]      Dev Fight Tools (Refill, Frame-Step, SlowMo, Stage)",
            "[F12]          Save PNG Framebuffer Screenshot to screenshots/"
        };
        drawPanelText(col2X, y + 26.0f, keyLines, 1.24f, CYAN, 1.42f);

        float stY = my + mh - 134.0f;
        renderer_.rect(mx + 24.0f, stY, mw - 48.0f, 88.0f, Color(0.03f, 0.035f, 0.048f, 0.95f));
        renderer_.rectOutline(mx + 24.0f, stY, mw - 48.0f, 88.0f, 1.0f, Color(0.24f, 0.26f, 0.32f, 0.9f));
        renderer_.rect(mx + 24.0f, stY, 3.0f, 88.0f, CYAN);
        renderer_.text(mx + 38.0f, stY + 10.0f, "LIVE ENGINE STATE", 1.35f, GOLD);

        char st1[200], st2[200];
        std::snprintf(st1, sizeof(st1), "ACTIVE PIPELINE [F2]:  %s", PIPE_NAMES[rm]);
        Vec3 sd = renderer_.sunDir();
        std::snprintf(st2, sizeof(st2),
                      "CAMERA [V]: %s   |   ANIMATION [P]: %s   |   SUN DIR [[/]]: (%.2f, %.2f, %.2f) [YAW %.0f DEG]",
                      orbitCamActive_ ? "3D ORBIT CAMERA" : "GAMEPLAY CAMERA",
                      animPaused_ ? "PAUSED" : "RUNNING",
                      sd.x, sd.y, sd.z, renderer_.lightYawDeg());
        renderer_.text(mx + 38.0f, stY + 34.0f, st1, 1.3f, WHITE);
        renderer_.text(mx + 38.0f, stY + 58.0f, st2, 1.25f, UI_SUBTEXT);

        renderer_.textCentered(width_ * 0.5f, my + mh - 28.0f,
                               "PRESS [F1] OR [H] TO CLOSE OVERLAY      PRESS [F2] TO CYCLE SHADER PIPELINE LIVE",
                               1.35f, GOLD);
    }
}

} // namespace dc
