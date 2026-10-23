// Fight scene presentation: arena, camera, effects, HUD.
#include "game/app.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <cctype>

namespace dc {

namespace {
const Color WHITE(1, 1, 1), GOLD(1.0f, 0.82f, 0.3f), RED(0.95f, 0.2f, 0.2f), DIM(0.55f, 0.55f, 0.6f);
std::string upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }
std::string displayName(const std::string& n) {
    if (n.size() > 2 && n[n.size() - 2] == ' ' && (n.back() == 'L' || n.back() == 'M' || n.back() == 'H')) return upper(n.substr(0, n.size() - 2));
    return upper(n);
}
}

void App::buildArena() {
    MeshBuilder b;
    b.addPlane(Vec3(0, 0, 0), 44, 30, 22, Vec3(0.30f, 0.28f, 0.30f), Vec3(0.24f, 0.22f, 0.25f));
    // raised rim marking the fighting line
    b.addBox(Vec3(0, -0.02f, 0), Vec3(19.0f, 0.04f, 6.0f), Vec3(0.34f, 0.3f, 0.3f));
    arenaFloor_.upload(b);

    b.clear();
    Vec3 wallCol(0.36f, 0.32f, 0.34f);
    b.addBox(Vec3(0, 3.5f, -9.0f), Vec3(44, 7, 1), wallCol);          // back wall
    b.addBox(Vec3(-16, 3.5f, 0), Vec3(1, 7, 30), wallCol);           // sides
    b.addBox(Vec3(16, 3.5f, 0), Vec3(1, 7, 30), wallCol);
    b.addBox(Vec3(0, 7.0f, -4.5f), Vec3(44, 0.6f, 10), wallCol * 0.8f); // ceiling beam
    arenaWalls_.upload(b);

    b.clear();
    for (int i = -4; i <= 4; ++i) {
        float x = i * 4.0f;
        b.addBox(Vec3(x, 3.0f, -8.2f), Vec3(0.8f, 6.0f, 0.8f), Vec3(0.45f, 0.42f, 0.44f));
        b.addBox(Vec3(x, 0.25f, -8.2f), Vec3(1.2f, 0.5f, 1.2f), Vec3(0.3f, 0.28f, 0.3f));
        b.addBox(Vec3(x, 5.9f, -8.2f), Vec3(1.2f, 0.4f, 1.2f), Vec3(0.3f, 0.28f, 0.3f));
    }
    // braziers on both sides
    for (int s = -1; s <= 1; s += 2) {
        b.addCylinder(Vec3(s * 11.0f, 0, -5.5f), 0.5f, 1.3f, 10, Vec3(0.2f, 0.18f, 0.16f));
        b.addCylinder(Vec3(s * 11.0f, 1.3f, -5.5f), 0.7f, 0.3f, 10, Vec3(0.3f, 0.25f, 0.2f));
    }
    // seating steps at the front edge (towards the camera)
    for (int k = 0; k < 3; ++k) b.addBox(Vec3(0, 0.2f + k * 0.4f, 9.5f + k * 1.2f), Vec3(44, 0.4f, 1.2f), Vec3(0.22f, 0.2f, 0.22f));
    arenaProps_.upload(b);
}

void App::startFight(int charA, int charB, FightMode mode, int difficulty) {
    const auto& ros = roster();
    fightMode_ = mode;
    sim_.init(&ros[charA], &ros[charB], 2);
    p1Ai_ = mode == FightMode::Demo;
    p2Ai_ = mode != FightMode::Versus;
    ai_[0].reset(2, 1234); ai_[1].reset(difficulty, 99 + (uint64_t)charB * 31);
    simAccum_ = 0; paused_ = false; pauseIndex_ = 0; showMoveList_ = false; resultIndex_ = 0; resultTimer_ = 0;
    particles_.clear(); floats_.clear(); banners_.clear();
    fightCamInit_ = false; standPosInit_ = false; fightTime_ = 0; slowmo_ = 1.0f;
    scene_ = Scene::Fight;
    input_.setMouseCaptured(false);
    processEvents();
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
            hudFlash_ = std::max(hudFlash_, 0.3f * s);
            break;
        }
        case FightEvent::Block:
            spawnSparks(e.pos, 6, Color(0.6f, 0.8f, 1), Color(1, 1, 1), 3.5f, 0.09f, 0.35f, Blend::Additive, 4.0f);
            if (!e.text.empty()) addFloat(e.pos + Vec3(0, 0.6f, 0), e.text, Color(0.7f, 0.85f, 1), 0.8f, 2);
            break;
        case FightEvent::Throw:
            spawnSparks(e.pos, 12, Color(1, 0.8f, 0.6f), Color(1, 0.4f, 0.3f), 5.0f, 0.14f, 0.5f, Blend::Additive, 8.0f);
            break;
        case FightEvent::Spark:
            spawnSparks(e.pos, (int)(8 * e.strength), sc, WHITE, 5.0f, 0.1f, 0.4f, Blend::Additive, 5.0f);
            break;
        case FightEvent::Dust:
            spawnSparks(e.pos + Vec3(0, 0.1f, 0), (int)(6 * e.strength), Color(0.55f, 0.5f, 0.45f, 0.5f), Color(0.4f, 0.37f, 0.33f, 0.4f), 1.8f, 0.25f, 0.6f, Blend::Alpha, -1.5f);
            for (size_t i = particles_.size() - (size_t)(6 * e.strength); i < particles_.size(); ++i) { particles_[i].growth = 0.8f; particles_[i].vel.y = std::fabs(particles_[i].vel.y) * 0.4f; }
            break;
        case FightEvent::Land:
            spawnSparks(e.pos + Vec3(0, 0.1f, 0), 14, Color(0.55f, 0.5f, 0.45f, 0.6f), Color(0.4f, 0.37f, 0.33f, 0.4f), 3.0f, 0.3f, 0.7f, Blend::Alpha, -1.0f);
            for (size_t i = particles_.size() - 14; i < particles_.size(); ++i) { particles_[i].growth = 1.0f; particles_[i].vel.y = std::fabs(particles_[i].vel.y) * 0.5f; }
            break;
        case FightEvent::Text:
            if (e.player < 0) addBanner(e.text, GOLD, 1.2f, 7);
            else addFloat(e.pos, e.text, sc, 1.1f, 2.2f);
            break;
        case FightEvent::MoveName: {
            Color col = e.strength >= 3 ? Color(1, 0.3f, 0.2f) : (e.strength >= 2 ? GOLD : sc);
            addFloat(e.pos + Vec3(0, 2.4f, 0), displayName(e.text), col, e.strength >= 2 ? 1.8f : 1.0f, e.strength >= 2 ? 3.0f : 2.0f);
            if (e.strength >= 2) {
                addBanner(e.strength >= 3 ? "GREAT HEAT ATTACK" : "HEART HEAT ATTACK", col, 1.4f, 4, 0.25f);
                const Fighter& f = sim_.fighter(e.player);
                spawnSparks(f.pos + Vec3(0, 1.0f, 0), 40, sc, WHITE, 7.0f, 0.18f, 0.9f, Blend::Additive, 0.0f);
                Particle flash; flash.pos = f.pos + Vec3(0, 1.0f, 0); flash.life = flash.maxLife = 0.35f; flash.size = 2.5f; flash.col = sc.withAlpha(0.8f); flash.growth = 8.0f;
                particles_.push_back(flash);
                hudFlash_ = 0.6f;
            }
            break;
        }
        case FightEvent::Projectile:
            spawnSparks(e.pos, 8, sc, WHITE, 2.0f, 0.1f, 0.4f, Blend::Additive, 0);
            break;
        case FightEvent::Clash:
            addBanner("RUSH MODE", Color(1, 0.4f, 0.2f), 2.0f, 7, 0.3f);
            addBanner("MASH ATTACK!", WHITE, 2.5f, 3, 0.42f);
            clashPulse_ = 1.0f;
            break;
        case FightEvent::StyleBreak:
            addBanner(e.text, Color(0.6f, 0.4f, 1.0f), 1.5f, 6, 0.35f);
            spawnSparks(e.pos, 30, sc, Color(0.3f, 0.2f, 0.5f), 6.0f, 0.2f, 0.8f, Blend::Additive, 2.0f);
            break;
        case FightEvent::FlashCancel: {
            addFloat(e.pos + Vec3(0, 1.2f, 0), "FLASH CANCEL", Color(1, 1, 0.5f), 1.0f, 2.4f);
            Particle flash; flash.pos = e.pos; flash.life = flash.maxLife = 0.25f; flash.size = 2.0f; flash.col = Color(1, 1, 0.7f, 0.9f); flash.growth = 10.0f;
            particles_.push_back(flash);
            spawnSparks(e.pos, 20, Color(1, 1, 0.6f), WHITE, 5.0f, 0.12f, 0.5f, Blend::Additive, 0);
            break;
        }
        case FightEvent::KO:
            addBanner(e.text, e.text == "DRAMATIC FINISH" ? Color(1, 0.35f, 0.2f) : RED, 2.2f, e.text.size() > 6 ? 6 : 9, 0.35f);
            hudFlash_ = 1.0f;
            break;
        case FightEvent::RoundStart:
            addBanner(e.text, WHITE, 1.5f, 6);
            break;
        case FightEvent::Tech:
            addFloat(e.pos, "THROW TECH", Color(0.7f, 0.9f, 1), 1.0f, 2.2f);
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
    bool matchOver = sim_.phase() == Phase::MatchOver;

    if (!matchOver && (p1.justPressed(BTN_BACK) || p1.justPressed(BTN_START) || p2.justPressed(BTN_START)) && fightMode_ != FightMode::Demo) {
        if (showMoveList_) showMoveList_ = false;
        else { paused_ = !paused_; pauseIndex_ = 0; }
    }
    if (paused_) {
        if (showMoveList_) { if (p1.justPressed(BTN_BACK) || p1.justPressed(BTN_LIGHT)) showMoveList_ = false; return; }
        int n = 3;
        if (p1.justPressed(BTN_DOWN) || p2.justPressed(BTN_DOWN)) pauseIndex_ = (pauseIndex_ + 1) % n;
        if (p1.justPressed(BTN_UP) || p2.justPressed(BTN_UP)) pauseIndex_ = (pauseIndex_ + n - 1) % n;
        if (p1.justPressed(BTN_LIGHT) || p2.justPressed(BTN_LIGHT) || p1.justPressed(BTN_START)) {
            if (pauseIndex_ == 0) paused_ = false;
            else if (pauseIndex_ == 1) showMoveList_ = true;
            else { paused_ = false; scene_ = Scene::Title; }
        }
        return;
    }

    // fixed-step simulation with slow motion on KO
    float scale = sim_.timeScale();
    slowmo_ = lerp(slowmo_, scale, std::min(1.0f, dt * 10.0f));
    simAccum_ += dt * slowmo_;
    const float STEP = 1.0f / 60.0f;
    int steps = 0;
    while (simAccum_ >= STEP && steps < 4) {
        SimInput a = p1Ai_ ? ai_[0].think(sim_, 0) : padToSim(p1);
        SimInput b = p2Ai_ ? ai_[1].think(sim_, 1) : padToSim(p2);
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
            if (resultTimer_ > 3.0f) { int a = fxRng_.range(0, 5), b = fxRng_.range(0, 4); if (b >= a) b++; startFight(a, b, FightMode::Demo, 2); }
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
            // defeat: retry or give up
            int n = 2;
            if (p1.justPressed(BTN_DOWN) || p1.justPressed(BTN_UP)) resultIndex_ = (resultIndex_ + 1) % n;
            if (p1.justPressed(BTN_START) || p1.justPressed(BTN_LIGHT)) {
                if (resultIndex_ == 0) startFight(huntPlayerChar_, hunt_.opponents()[huntEngaged_].charId, FightMode::Hunt, huntDefeatedCount_ < 2 ? 0 : (huntDefeatedCount_ < 4 ? 1 : 2));
                else { scene_ = Scene::Title; }
            }
            return;
        }
        int n = 3;
        if (p1.justPressed(BTN_DOWN) || p2.justPressed(BTN_DOWN)) resultIndex_ = (resultIndex_ + 1) % n;
        if (p1.justPressed(BTN_UP) || p2.justPressed(BTN_UP)) resultIndex_ = (resultIndex_ + n - 1) % n;
        if (p1.justPressed(BTN_START) || p1.justPressed(BTN_LIGHT) || p2.justPressed(BTN_START) || p2.justPressed(BTN_LIGHT)) {
            if (resultIndex_ == 0) startFight(sim_.fighter(0).ch - &roster()[0], sim_.fighter(1).ch - &roster()[0], fightMode_, 1);
            else if (resultIndex_ == 1) { scene_ = Scene::Select; selectFor_ = 0; }
            else scene_ = Scene::Title;
        }
    }
}

void App::renderFight() {
    const Fighter& a = sim_.fighter(0);
    const Fighter& b = sim_.fighter(1);
    const Character& ca = *a.ch; const Character& cb = *b.ch;

    // ---- camera
    float midX = clampf((a.pos.x + b.pos.x) * 0.5f, -6.3f, 6.3f);
    float dist = std::fabs(a.pos.x - b.pos.x);
    float highY = std::max(a.pos.y, b.pos.y);
    float zoom = clampf(std::max(dist, highY * 2.2f), 2.4f, 9.0f);
    float midY = (a.pos.y + b.pos.y) * 0.5f;
    Camera want;
    want.target = Vec3(midX, 1.05f + midY * 0.45f + highY * 0.2f + zoom * 0.1f, (a.pos.z + b.pos.z) * 0.25f);
    want.eye = want.target + Vec3(0, 0.75f + zoom * 0.1f, 4.9f + zoom * 0.62f);
    want.fovDeg = 42.0f;
    if (sim_.phase() == Phase::Intro) {
        float t = (float)sim_.phaseTimer() / 150.0f;
        want.eye = want.target + Vec3(std::sin(t * 2.0f) * 3.0f, 1.2f - t * 0.5f, 5.5f + (1.0f - t) * 2.0f);
    }
    if (!fightCamInit_) { fightCam_ = want; fightCamInit_ = true; }
    else {
        float k = std::min(1.0f, (1.0f / 60.0f) * 9.0f);
        fightCam_.eye = lerp(fightCam_.eye, want.eye, k);
        fightCam_.target = lerp(fightCam_.target, want.target, k);
        fightCam_.fovDeg = lerp(fightCam_.fovDeg, want.fovDeg, k);
    }
    Camera cam = fightCam_;
    float sh = sim_.shake();
    if (sh > 0) {
        Vec3 off(fxRng_.uniform(-1, 1) * sh * 0.18f, fxRng_.uniform(-1, 1) * sh * 0.14f, 0);
        cam.eye += off; cam.target += off;
    }

    Color fog(0.05f, 0.04f, 0.06f);
    renderer_.beginFrame(cam, fog);
    renderer_.setDirectionalLight(Vec3(-0.35f, -1.0f, -0.45f), Vec3(1.0f, 0.95f, 0.88f) * 0.9f, 0.32f);
    renderer_.setShadowFocus(Vec3(midX, 0, 0), 11.0f);
    renderer_.setFog(Vec3(0.05f, 0.04f, 0.06f), 0.0015f);
    renderer_.addPointLight(Vec3(-6, 3.5f, 1.5f), ca.styleColor.rgb() * 1.1f, 12);
    renderer_.addPointLight(Vec3(6, 3.5f, 1.5f), cb.styleColor.rgb() * 1.1f, 12);
    renderer_.addPointLight(Vec3(-11, 1.9f, -5.5f), Vec3(1.0f, 0.55f, 0.2f) * (1.0f + 0.2f * std::sin(time_ * 9)), 9);
    renderer_.addPointLight(Vec3(11, 1.9f, -5.5f), Vec3(1.0f, 0.55f, 0.2f) * (1.0f + 0.2f * std::cos(time_ * 7)), 9);
    if (hudFlash_ > 0) renderer_.addPointLight(Vec3(midX, 2.5f, 1), Vec3(1, 1, 1) * hudFlash_ * 2.0f, 10);

    renderer_.draw(arenaFloor_, Mat4::identity(), Color(1, 1, 1), 0, 1);
    renderer_.draw(arenaWalls_, Mat4::identity(), Color(1, 1, 1), 0, 2);
    renderer_.draw(arenaProps_, Mat4::identity(), Color(1, 1, 1), 0, 3);
    // pillar accents in the opponent's colour
    for (int i = -4; i <= 4; ++i)
        renderer_.draw(rig_.unitBox(), Mat4::translate(Vec3(i * 4.0f, 3.0f, -7.75f)) * Mat4::scale(Vec3(0.3f, 4.5f, 0.06f)), (i % 2 ? cb.styleColor : ca.styleColor) * 0.9f, 0.9f, 0, false);
    // brazier flames
    for (int s = -1; s <= 1; s += 2) {
        Vec3 fp(s * 11.0f, 1.7f, -5.5f);
        renderer_.drawBillboard(fp + Vec3(0, 0.15f * std::sin(time_ * 12 + s), 0), 1.4f, Color(1.0f, 0.55f, 0.15f, 0.6f), Blend::Additive, time_ * 3);
        renderer_.drawBillboard(fp, 0.7f, Color(1.0f, 0.9f, 0.5f, 0.9f), Blend::Additive, -time_ * 2);
    }
    // stage boundary glow
    for (int s = -1; s <= 1; s += 2) {
        Vec3 wp(s * (FightSim::STAGE_HALF + 0.5f), 1.2f, 0);
        renderer_.draw(rig_.unitBox(), Mat4::translate(wp) * Mat4::scale(Vec3(0.05f, 2.4f, 4.0f)), Color(0.6f, 0.7f, 1.0f, 0.12f), 1.0f, 0, false);
    }

    // ---- fighters
    const Fighter* fs[2] = {&a, &b};
    for (int i = 0; i < 2; ++i) {
        const Fighter& f = *fs[i];
        const Character& c = *f.ch;
        float yaw = f.facing * radians(70.0f);
        Pose p = poseForFighter(f, fightTime_ + i * 0.37f);
        float hs = (float)f.hitstop;
        Vec3 pos = f.pos;
        if (hs > 0 && f.inHitstun()) pos.x += std::sin(fightTime_ * 90.0f) * 0.03f;
        float flash = f.flash * 0.8f;
        if (f.state == FState::StyleBreak) flash = 0.25f + 0.25f * std::sin(fightTime_ * 20);
        bool modeOn = c.style == StyleType::Mode && f.styleOn;
        rig_.draw(renderer_, p, pos, yaw, c, 1.0f, 1.0f, flash);
        if (modeOn) {
            renderer_.drawBillboard(pos + Vec3(0, 1.0f, 0), 2.6f + 0.2f * std::sin(fightTime_ * 10), c.styleColor.withAlpha(0.22f), Blend::Additive, fightTime_ * 2);
            if (fxRng_.chance(0.4f)) {
                Particle s; s.pos = pos + Vec3(fxRng_.uniform(-0.4f, 0.4f), fxRng_.uniform(0.1f, 1.8f), fxRng_.uniform(-0.3f, 0.3f));
                s.vel = Vec3(0, fxRng_.uniform(0.8f, 2.0f), 0); s.life = s.maxLife = 0.6f; s.size = 0.08f; s.col = c.styleColor.withAlpha(0.8f);
                particles_.push_back(s);
            }
        }
        // Stand
        if (c.style == StyleType::Stand && f.styleOn) {
            float blend = f.standAttackBlend;
            Vec3 behind(pos.x - f.facing * 0.85f, 0.3f + 0.08f * std::sin(fightTime_ * 2.5f + i), pos.z + 0.45f);
            Vec3 front(pos.x + f.facing * 0.95f, pos.y + 0.05f, pos.z);
            Vec3 target = lerp(behind, front, blend);
            if (!standPosInit_) { standPos_[0] = standPos_[1] = target; }
            standPos_[i] = lerp(standPos_[i], target, 0.35f);
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
            renderer_.drawBillboard(standPos_[i] + Vec3(0, 1.3f, 0), 2.4f, c.styleColor.withAlpha(0.16f), Blend::Additive, fightTime_ * 1.5f);
            if (fxRng_.chance(0.3f)) {
                Particle s; s.pos = standPos_[i] + Vec3(fxRng_.uniform(-0.5f, 0.5f), fxRng_.uniform(0.2f, 2.0f), fxRng_.uniform(-0.3f, 0.3f));
                s.vel = Vec3(0, fxRng_.uniform(0.3f, 1.0f), 0); s.life = s.maxLife = 0.8f; s.size = 0.06f; s.col = c.styleColor.withAlpha(0.7f);
                particles_.push_back(s);
            }
        }
        // style gauge almost empty: warning sparkles on the Stand user
        // blocking shield flash
        if (f.state == FState::Block || f.state == FState::CrouchBlock) {
            float k = 1.0f - (float)f.timer / (float)std::max(1, f.stateLen);
            renderer_.drawBillboard(pos + Vec3(f.facing * 0.5f, 1.0f, 0.1f), 1.0f, Color(0.5f, 0.75f, 1.0f, 0.35f * k), Blend::Additive, fightTime_ * 4);
        }
        if (f.invulStrikes > 0 && (f.state == FState::Sidestep || f.state == FState::Backdash)) {
            renderer_.drawBillboard(pos + Vec3(0, 1.0f, 0), 1.6f, Color(0.8f, 0.9f, 1.0f, 0.2f), Blend::Additive, fightTime_ * 6);
        }
    }
    standPosInit_ = true;

    // ---- projectiles
    for (const Projectile& p : sim_.projectiles()) {
        Mat4 m = Mat4::translate(p.pos) * Mat4::rotateY(p.spin) * Mat4::rotateZ(p.spin * 0.7f) * Mat4::scale(Vec3(0.5f, 0.5f, 0.5f));
        renderer_.draw(projectileMesh_, m, mix(p.color, WHITE, 0.3f), 1.0f, 0, false);
        renderer_.drawBillboard(p.pos, 1.4f, p.color.withAlpha(0.5f), Blend::Additive, p.spin);
        Particle t; t.pos = p.pos + Vec3(fxRng_.uniform(-0.1f, 0.1f), fxRng_.uniform(-0.1f, 0.1f), fxRng_.uniform(-0.1f, 0.1f));
        t.vel = Vec3(-p.vx * 8.0f, fxRng_.uniform(-0.5f, 0.5f), 0); t.life = t.maxLife = 0.35f; t.size = 0.18f; t.col = p.color.withAlpha(0.8f);
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

    drawFightHud();
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
        if (!project(f.pos, sx, sy)) continue;
        float k = clampf(f.life / f.maxLife, 0, 1);
        float al = k < 0.3f ? k / 0.3f : 1.0f;
        renderer_.textCentered(sx + 2, sy + 2, f.text, f.scale, Color(0, 0, 0, 0.6f * al));
        renderer_.textCentered(sx, sy, f.text, f.scale, f.col.withAlpha(al));
    }

    // health bars
    float barY = 26, barH = 24, margin = 40, barW = cx - margin - 70;
    for (int i = 0; i < 2; ++i) {
        const Fighter& f = *fs[i];
        const Character& c = *f.ch;
        float hpFrac = clampf(f.hpShown / c.hp, 0, 1), redFrac = clampf(f.hpRedShown / c.hp, 0, 1);
        float x0 = i == 0 ? margin : cx + 70;
        renderer_.rect(x0 - 3, barY - 3, barW + 6, barH + 6, Color(0, 0, 0, 0.7f));
        renderer_.rect(x0, barY, barW, barH, Color(0.18f, 0.05f, 0.06f));
        float rw = barW * redFrac, hw = barW * hpFrac;
        float rx = i == 0 ? x0 : x0 + barW - rw;
        float hx = i == 0 ? x0 : x0 + barW - hw;
        renderer_.rect(rx, barY, rw, barH, Color(0.8f, 0.15f, 0.1f));
        Color hpCol = hpFrac > 0.5f ? Color(0.95f, 0.85f, 0.2f) : (hpFrac > 0.25f ? Color(0.95f, 0.5f, 0.15f) : Color(0.9f, 0.15f, 0.15f));
        if (f.state == FState::StyleBreak) hpCol = mix(hpCol, Color(0.6f, 0.4f, 1.0f), 0.5f + 0.5f * std::sin(time_ * 15));
        renderer_.rect(hx, barY, hw, barH, hpCol);
        renderer_.rect(hx, barY, hw, 5, WHITE.withAlpha(0.25f));
        // name
        float ny = barY + barH + 8;
        if (i == 0) renderer_.text(x0, ny, c.name, 2.4f, WHITE);
        else renderer_.textRight(x0 + barW, ny, c.name, 2.4f, WHITE);
        // style gauge
        float sgW = barW * 0.5f, sgY = ny + 24;
        float sx0 = i == 0 ? x0 : x0 + barW - sgW;
        renderer_.rect(sx0, sgY, sgW, 7, Color(0.1f, 0.1f, 0.12f, 0.8f));
        float sw = sgW * clampf(f.styleGauge, 0, 1);
        renderer_.rect(i == 0 ? sx0 : sx0 + sgW - sw, sgY, sw, 7, f.styleOn ? c.styleColor : c.styleColor * 0.55f);
        std::string sl = c.styleName + (f.styleOn ? "  ON" : "");
        if (i == 0) renderer_.text(sx0, sgY + 10, sl, 1.4f, f.styleOn ? c.styleColor : DIM);
        else renderer_.textRight(sx0 + sgW, sgY + 10, sl, 1.4f, f.styleOn ? c.styleColor : DIM);
        // round pips
        for (int r = 0; r < sim_.roundsToWin(); ++r) {
            float px = i == 0 ? x0 + barW - 16 - r * 20 : x0 + 4 + r * 20;
            renderer_.rect(px, ny + 4, 12, 12, r < f.roundsWon ? GOLD : Color(0.2f, 0.2f, 0.22f));
        }
        // Heart Heat Gauge (bottom)
        float segW = 92, segH = 16, gap = 6, hy = H - 48;
        float totalW = segW * 3 + gap * 2;
        float hx0 = i == 0 ? margin : W - margin - totalW;
        int bars = (int)(f.hhg / 1000.0f);
        for (int s = 0; s < 3; ++s) {
            float sx = hx0 + s * (segW + gap);
            renderer_.rect(sx - 2, hy - 2, segW + 4, segH + 4, Color(0, 0, 0, 0.7f));
            renderer_.rect(sx, hy, segW, segH, Color(0.12f, 0.1f, 0.12f));
            float fill = clampf((f.hhg - s * 1000.0f) / 1000.0f, 0, 1);
            Color col = s < bars ? mix(Color(1.0f, 0.4f, 0.1f), Color(1, 0.9f, 0.4f), 0.5f + 0.5f * std::sin(time_ * 6 + s)) : Color(0.9f, 0.3f, 0.15f);
            if (f.hhg >= 3000) col = mix(GOLD, WHITE, 0.5f + 0.5f * std::sin(time_ * 8));
            renderer_.rect(sx, hy, segW * fill, segH, col);
        }
        std::string hl = "HEAT " + std::to_string(bars);
        if (i == 0) renderer_.text(hx0, hy - 22, hl, 1.8f, GOLD);
        else renderer_.textRight(hx0 + totalW, hy - 22, hl, 1.8f, GOLD);
        // combo counter
        if (f.comboShow > 0 && f.comboHits >= 2) {
            float k = clampf(f.comboShow / 30.0f, 0, 1);
            float cxp = i == 0 ? margin + 10 : W - margin - 10;
            std::string hits = std::to_string(f.comboHits) + " HITS";
            std::string dmg = std::to_string((int)f.comboDamage) + " DMG";
            Color cc = f.comboHits >= 8 ? Color(1, 0.3f, 0.3f) : (f.comboHits >= 5 ? GOLD : WHITE);
            if (i == 0) { renderer_.text(cxp, H * 0.33f, hits, 4, cc.withAlpha(k)); renderer_.text(cxp, H * 0.33f + 36, dmg, 2, DIM.withAlpha(k)); }
            else { renderer_.textRight(cxp, H * 0.33f, hits, 4, cc.withAlpha(k)); renderer_.textRight(cxp, H * 0.33f + 36, dmg, 2, DIM.withAlpha(k)); }
        }
        // move name on the side
        if (f.lastMoveNameTimer > 0 && f.lastMoveNameTimer < 70) {
            float k = clampf(f.lastMoveNameTimer / 20.0f, 0, 1);
            if (i == 0) renderer_.text(margin, H * 0.33f + 70, displayName(f.lastMoveName), 2, f.ch->styleColor.withAlpha(k));
            else renderer_.textRight(W - margin, H * 0.33f + 70, displayName(f.lastMoveName), 2, f.ch->styleColor.withAlpha(k));
        }
    }
    // timer
    renderer_.rect(cx - 48, 16, 96, 44, Color(0, 0, 0, 0.7f));
    int secs = sim_.secondsLeft();
    char buf[16]; std::snprintf(buf, sizeof(buf), "%02d", secs);
    renderer_.textCentered(cx, 22, buf, 4, secs <= 10 ? RED : WHITE);
    renderer_.textCentered(cx, 64, "ROUND " + std::to_string(sim_.round()), 1.6f, DIM);

    // rush clash meter
    if (sim_.clash().active) {
        const ClashState& c = sim_.clash();
        float total = (float)std::max(1, c.presses[0] + c.presses[1]);
        float frac = c.presses[0] / total;
        float mw = 420, mh = 22, mx = cx - mw * 0.5f, my = H * 0.52f;
        renderer_.rect(mx - 3, my - 3, mw + 6, mh + 6, Color(0, 0, 0, 0.8f));
        renderer_.rect(mx, my, mw * frac, mh, a.ch->styleColor);
        renderer_.rect(mx + mw * frac, my, mw * (1 - frac), mh, b.ch->styleColor);
        float timeLeft = (150 - c.timer) / 60.0f;
        std::snprintf(buf, sizeof(buf), "%.1f", timeLeft);
        renderer_.textCentered(cx, my + 30, buf, 2, WHITE);
    }

    // banners
    for (const Banner& bn : banners_) {
        float k = clampf(bn.life / bn.maxLife, 0, 1);
        float al = k < 0.25f ? k / 0.25f : 1.0f;
        float pop = 1.0f + 0.25f * std::max(0.0f, (k - 0.9f) / 0.1f);
        float y = H * bn.yFrac;
        renderer_.textCentered(cx + 4, y + 4, bn.text, bn.scale * pop, Color(0, 0, 0, 0.7f * al));
        renderer_.textCentered(cx, y, bn.text, bn.scale * pop, bn.col.withAlpha(al));
    }
    // hit flash vignette
    if (hudFlash_ > 0) renderer_.rect(0, 0, W, H, Color(1, 1, 1, 0.12f * hudFlash_));

    // intro: show style names
    if (sim_.phase() == Phase::Intro && sim_.phaseTimer() < 110) {
        float al = clampf((110 - sim_.phaseTimer()) / 25.0f, 0, 1);
        renderer_.text(margin, H * 0.6f, a.ch->name, 4, WHITE.withAlpha(al));
        renderer_.text(margin, H * 0.6f + 40, a.ch->styleName, 2, a.ch->styleColor.withAlpha(al));
        renderer_.textRight(W - margin, H * 0.6f, b.ch->name, 4, WHITE.withAlpha(al));
        renderer_.textRight(W - margin, H * 0.6f + 40, b.ch->styleName, 2, b.ch->styleColor.withAlpha(al));
        renderer_.textCentered(cx, H * 0.6f + 20, "VS", 5, RED.withAlpha(al));
    }
    // match over
    if (sim_.phase() == Phase::MatchOver && resultTimer_ > 0.8f) {
        float al = clampf((resultTimer_ - 0.8f) * 2.0f, 0, 1);
        renderer_.rect(0, H * 0.3f, W, 240, Color(0, 0, 0, 0.7f * al));
        int w = sim_.matchWinner();
        const Character& wc = *sim_.fighter(w).ch;
        renderer_.textCentered(cx, H * 0.3f + 20, wc.name + " WINS", 6, wc.styleColor.withAlpha(al));
        if (fightMode_ == FightMode::Hunt) {
            if (w == 0) renderer_.textCentered(cx, H * 0.3f + 90, "RETURNING TO THE HALLS...", 2.5f, WHITE.withAlpha(al));
            else {
                renderer_.textCentered(cx, H * 0.3f + 80, "YOU WERE FOUND WANTING", 2.5f, RED.withAlpha(al));
                const char* items[2] = {"RETRY THE FIGHT", "GIVE UP"};
                for (int i = 0; i < 2; ++i) renderer_.textCentered(cx, H * 0.3f + 130 + i * 36, items[i], 2.6f, (i == resultIndex_ ? GOLD : WHITE).withAlpha(al));
            }
        } else if (fightMode_ != FightMode::Demo) {
            const char* items[3] = {"REMATCH", "CHARACTER SELECT", "TITLE"};
            for (int i = 0; i < 3; ++i) renderer_.textCentered(cx, H * 0.3f + 100 + i * 36, items[i], 2.6f, (i == resultIndex_ ? GOLD : WHITE).withAlpha(al));
        }
    }
    if (paused_) { if (showMoveList_) drawMoveList(); else drawPauseMenu(); }
}

void App::drawPauseMenu() {
    float W = (float)width_, H = (float)height_, cx = W * 0.5f;
    renderer_.rect(0, 0, W, H, Color(0, 0, 0, 0.65f));
    renderer_.textCentered(cx, H * 0.3f, "PAUSED", 6, WHITE);
    const char* items[3] = {"RESUME", "MOVE LIST", "QUIT TO TITLE"};
    for (int i = 0; i < 3; ++i) renderer_.textCentered(cx, H * 0.45f + i * 42, items[i], 3, i == pauseIndex_ ? GOLD : WHITE);
    renderer_.textCentered(cx, H * 0.85f, "J / ENTER  SELECT", 1.6f, DIM);
}

void App::drawMoveList() {
    float W = (float)width_, H = (float)height_;
    renderer_.rect(0, 0, W, H, Color(0, 0, 0, 0.8f));
    const Character& c = *sim_.fighter(0).ch;
    float x = 60, y = 40;
    renderer_.text(x, y, c.name + "  -  " + c.styleName, 3, c.styleColor);
    std::vector<std::string> lines = {
        "SYSTEM                                        (F = TOWARD OPPONENT, B = AWAY)",
        "LIGHT J   MEDIUM K   HEAVY L        CHAIN  L > M > H    EASY BEAT: TAP J REPEATEDLY",
        "THROW  J+K                          TAUNT  O  (DRAINS HEAT OF A DOWNED FOE)",
        "DASH   U + F / B  OR DOUBLE TAP     SIDESTEP (STYLISH EVADE)  U + W / S",
        "STAND / MODE ON-OFF  I              STAND ON: LONGER NORMALS, STAND TAKES HITS",
        "FLASH CANCEL  J+K+L DURING A MOVE   COSTS 1 HEAT BAR",
        "HHA  D,DF,F + TWO ATTACKS  (1 BAR)  GHA  D,DF,F + THREE ATTACKS  (2 BARS)",
        "RUSH MODE: WHEN RUSH ATTACKS COLLIDE, MASH ATTACK TO WIN THE CLASH",
        "",
        "SPECIAL MOVES",
    };
    for (const Move& m : c.moves) {
        if (m.kind == MoveKind::Special && m.buttons == BTN_LIGHT) {
            std::string mo = m.motion == Motion::QCF ? "D,DF,F + ATK" : m.motion == Motion::QCB ? "D,DB,B + ATK" : "F,D,DF + ATK";
            std::string tag = m.projectile ? "  (PROJECTILE)" : m.commandThrow ? "  (COMMAND THROW)" : m.rush ? "  (RUSH)" : m.armor ? "  (ARMOR)" : "";
            lines.push_back(upper(m.name.substr(0, m.name.size() - 2)) + "   " + mo + tag);
        }
    }
    for (const Move& m : c.moves) if (m.kind == MoveKind::HHA) lines.push_back("HHA  " + m.name);
    for (const Move& m : c.moves) if (m.kind == MoveKind::GHA) lines.push_back("GHA  " + m.name);
    drawPanelText(x, y + 50, lines, 1.7f, WHITE, 1.55f);
    renderer_.textCentered(W * 0.5f, H - 40, "ESC / J  BACK", 1.6f, DIM);
}

} // namespace dc
