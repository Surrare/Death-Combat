#include "game/hunt.h"
#include "game/characters.h"
#include <algorithm>
#include <cstdlib>

namespace dc {

namespace {
const float PLAYER_RADIUS = 0.36f;
const float WALK_SPEED = 4.2f;
const float RUN_SPEED = 7.4f;
}

void Hunt::init(int playerChar, const std::vector<int>& opponentChars, uint64_t seed) {
    rng_ = Rng(seed);
    playerChar_ = playerChar;
    int w = 11, h = 11;
    maze_.generate(w, h, seed);
    MeshBuilder fb, wb, cb, pb;
    maze_.buildMesh(fb, wb, cb, pb);
    floor_.upload(fb); walls_.upload(wb); ceiling_.upload(cb); props_.upload(pb);

    playerPos_ = maze_.cellCenter(w / 2, h / 2);
    camYaw_ = 0; playerYaw_ = 0;

    // scatter opponents in far dead-ends / corners
    std::vector<int> dist = maze_.pathDistances(maze_.cellIndex(w / 2, h / 2));
    std::vector<int> candidates;
    for (int cz = 0; cz < h; ++cz)
        for (int cx = 0; cx < w; ++cx) {
            int d = dist[maze_.cellIndex(cx, cz)];
            if (d >= 8) candidates.push_back(maze_.cellIndex(cx, cz));
        }
    opps_.clear();
    for (int id : opponentChars) {
        HuntOpponent o;
        o.charId = id;
        int cell = candidates.empty() ? maze_.cellIndex(0, 0) : candidates[rng_.range(0, (int)candidates.size() - 1)];
        // keep opponents apart
        for (int tries = 0; tries < 20 && !candidates.empty(); ++tries) {
            bool ok = true;
            for (const HuntOpponent& other : opps_) {
                int ocx, ocz; maze_.cellOf(other.pos, ocx, ocz);
                int cx = cell % w, cz = cell / w;
                if (std::abs(ocx - cx) + std::abs(ocz - cz) < 3) ok = false;
            }
            if (ok) break;
            cell = candidates[rng_.range(0, (int)candidates.size() - 1)];
        }
        o.pos = maze_.cellCenter(cell % w, cell / w);
        o.yaw = rng_.uniform(0, 2 * PI);
        o.idleTimer = rng_.uniform(0.5f, 3.0f);
        o.speedMul = rng_.uniform(0.85f, 1.15f);
        opps_.push_back(o);
    }
    time_ = 0; senseLevel_ = 0; heartPulse_ = 0; heartTimer_ = 0; senseDir_ = -1; engageCandidate_ = -1;
    updateSense();
    updateCamera(0.0f, PadState());
}

int Hunt::remaining() const {
    int n = 0;
    for (const HuntOpponent& o : opps_) if (!o.defeated) ++n;
    return n;
}

bool Hunt::anyOpponentHunting() const {
    for (const HuntOpponent& o : opps_) if (!o.defeated && o.alert > 0.5f) return true;
    return false;
}

void Hunt::markDefeated(int idx) {
    if (idx >= 0 && idx < (int)opps_.size()) opps_[idx].defeated = true;
}

void Hunt::resumeAfterFight() {
    // push the remaining opponents away from the player so you don't get re-engaged instantly
    for (HuntOpponent& o : opps_) {
        if (o.defeated) continue;
        o.alert = 0; o.losLost = 0; o.targetCell = -1; o.idleTimer = 1.5f;
        if (distance(o.pos, playerPos_) < 6.0f) {
            int cx, cz; maze_.cellOf(playerPos_, cx, cz);
            std::vector<int> dist = maze_.pathDistances(maze_.cellIndex(cx, cz));
            int best = -1, bestD = -1;
            for (int i = 0; i < (int)dist.size(); ++i) if (dist[i] >= 5 && dist[i] > bestD && rng_.chance(0.5f)) { bestD = dist[i]; best = i; }
            if (best >= 0) o.pos = maze_.cellCenter(best % maze_.width(), best / maze_.width());
        }
    }
    senseRefresh_ = 0;
}

void Hunt::updateOpponent(HuntOpponent& o, float dt) {
    if (o.defeated) return;
    float d = distance(o.pos, playerPos_);
    bool los = d < 10.0f && maze_.lineOfSight(o.pos + Vec3(0, 1, 0), playerPos_ + Vec3(0, 1, 0));
    if (los) { o.alert = std::min(1.0f, o.alert + dt * (d < 4.0f ? 2.0f : 0.8f)); o.losLost = 0; }
    else { o.losLost += dt; if (o.losLost > 3.0f) o.alert = std::max(0.0f, o.alert - dt * 0.4f); }

    Vec3 moveDir;
    float speed = 0;
    if (o.alert > 0.5f) {
        // chase: head straight at the player when visible, otherwise toward the player's cell by BFS
        if (los) moveDir = normalize(Vec3(playerPos_.x - o.pos.x, 0, playerPos_.z - o.pos.z));
        else {
            int pcx, pcz; maze_.cellOf(playerPos_, pcx, pcz);
            int ocx, ocz; maze_.cellOf(o.pos, ocx, ocz);
            std::vector<int> dist = maze_.pathDistances(maze_.cellIndex(pcx, pcz));
            int best = -1, bestD = 1 << 20;
            for (int n : maze_.neighbours(ocx, ocz)) if (dist[n] >= 0 && dist[n] < bestD) { bestD = dist[n]; best = n; }
            Vec3 target = best >= 0 ? maze_.cellCenter(best % maze_.width(), best / maze_.width()) : o.pos;
            if (bestD >= dist[maze_.cellIndex(ocx, ocz)] && dist[maze_.cellIndex(ocx, ocz)] >= 0) target = maze_.cellCenter(pcx, pcz);
            moveDir = normalize(Vec3(target.x - o.pos.x, 0, target.z - o.pos.z));
        }
        speed = 3.4f * o.speedMul;
    } else {
        // wander between cells
        if (o.targetCell < 0) {
            if (o.idleTimer > 0) o.idleTimer -= dt;
            else {
                int cx, cz; maze_.cellOf(o.pos, cx, cz);
                std::vector<int> nb = maze_.neighbours(cx, cz);
                if (!nb.empty()) o.targetCell = nb[rng_.range(0, (int)nb.size() - 1)];
            }
        }
        if (o.targetCell >= 0) {
            Vec3 t = maze_.cellCenter(o.targetCell % maze_.width(), o.targetCell / maze_.width());
            Vec3 to = Vec3(t.x - o.pos.x, 0, t.z - o.pos.z);
            if (to.length() < 0.25f) { o.targetCell = -1; o.idleTimer = rng_.uniform(0.3f, 2.5f); }
            else { moveDir = normalize(to); speed = 1.7f * o.speedMul; }
        }
    }
    o.moving = speed > 0;
    if (o.moving) {
        o.pos += moveDir * (speed * dt);
        o.pos = maze_.collideCircle(o.pos, 0.36f);
        float want = yawToward(moveDir);
        o.yaw += wrapAngle(want - o.yaw) * std::min(1.0f, dt * 8.0f);
        o.walkPhase += dt * (speed > 3.0f ? 11.0f : 7.0f);
    } else if (o.alert > 0.5f) {
        o.yaw += wrapAngle(yawToward(Vec3(playerPos_.x - o.pos.x, 0, playerPos_.z - o.pos.z)) - o.yaw) * std::min(1.0f, dt * 6.0f);
    }
}

void Hunt::updateSense() {
    int pcx, pcz; maze_.cellOf(playerPos_, pcx, pcz);
    int pcell = maze_.cellIndex(pcx, pcz);
    std::vector<int> dist = maze_.pathDistances(pcell);
    int bestD = 1 << 20; int best = -1;
    for (int i = 0; i < (int)opps_.size(); ++i) {
        const HuntOpponent& o = opps_[i];
        if (o.defeated) continue;
        int cx, cz; maze_.cellOf(o.pos, cx, cz);
        int d = dist[maze_.cellIndex(cx, cz)];
        if (d >= 0 && d < bestD) { bestD = d; best = i; }
    }
    if (best < 0) { senseLevel_ = 0; senseDir_ = -1; return; }
    float eu = distance(opps_[best].pos, playerPos_) / Maze::CELL;
    float pd = (float)bestD;
    float measure = std::max(std::min(pd, eu), eu * 0.5f);
    senseLevel_ = clampf(1.0f - measure / 15.0f, 0.0f, 1.0f);
    // next step direction: neighbour with the smallest distance to the opponent
    int ocx, ocz; maze_.cellOf(opps_[best].pos, ocx, ocz);
    std::vector<int> fromOpp = maze_.pathDistances(maze_.cellIndex(ocx, ocz));
    int bestN = fromOpp[pcell]; senseDir_ = -1;
    if (bestN == 0) { senseDir_ = -1; return; }
    const int dirs[4] = {Maze::N, Maze::E, Maze::S, Maze::W};
    const int dxs[4] = {0, 1, 0, -1}, dzs[4] = {-1, 0, 1, 0};
    for (int k = 0; k < 4; ++k) {
        if (maze_.hasWall(pcx, pcz, dirs[k])) continue;
        int nx = pcx + dxs[k], nz = pcz + dzs[k];
        if (!maze_.inBounds(nx, nz)) continue;
        int d = fromOpp[maze_.cellIndex(nx, nz)];
        if (d >= 0 && d < bestN) { bestN = d; senseDir_ = k; }
    }
}

void Hunt::updateCamera(float dt, const PadState& pad) {
    camYaw_ += pad.lookX * 0.03f;
    camPitch_ = clampf(camPitch_ + pad.lookY * 0.02f, -0.15f, 0.9f);
    Vec3 fwd(std::sin(camYaw_), 0, -std::cos(camYaw_));
    Vec3 target = playerPos_ + Vec3(0, 1.35f, 0);
    float dist = 3.4f;
    Vec3 want = target - fwd * (dist * std::cos(camPitch_)) + Vec3(0, dist * std::sin(camPitch_), 0);
    // pull the camera in front of walls
    Vec3 eye = want;
    Vec3 d = want - target;
    float len = d.length();
    int steps = (int)(len / 0.1f) + 1;
    for (int i = 1; i <= steps; ++i) {
        Vec3 p = target + d * ((float)i / steps);
        if (maze_.pointInWall(p, 0.3f) || p.y > Maze::HEIGHT - 0.3f) { eye = target + d * ((float)(i - 1) / steps); break; }
        eye = p;
    }
    if (dt > 0) {
        cam_.eye = lerp(cam_.eye, eye, std::min(1.0f, dt * 14.0f));
        cam_.target = lerp(cam_.target, target, std::min(1.0f, dt * 16.0f));
    } else { cam_.eye = eye; cam_.target = target; }
    cam_.fovDeg = 58.0f;
    cam_.znear = 0.05f; cam_.zfar = 90.0f;
}

int Hunt::update(float dt, const PadState& pad, bool engageAllowed) {
    time_ += dt;
    // movement relative to the camera
    Vec3 fwd(std::sin(camYaw_), 0, -std::cos(camYaw_));
    Vec3 right(std::cos(camYaw_), 0, std::sin(camYaw_));
    Vec3 move = fwd * pad.axisY + right * pad.axisX;
    float ml = move.length();
    running_ = pad.down(BTN_DASH);
    if (ml > 1.0f) move = move / ml;
    if (ml > 0.05f) {
        float speed = running_ ? RUN_SPEED : WALK_SPEED;
        playerPos_ += move * (speed * dt);
        playerPos_ = maze_.collideCircle(playerPos_, PLAYER_RADIUS);
        float want = yawToward(move);
        playerYaw_ += wrapAngle(want - playerYaw_) * std::min(1.0f, dt * 12.0f);
        walkPhase_ += dt * (running_ ? 12.0f : 8.0f);
        moveBlend_ = std::min(1.0f, moveBlend_ + dt * 6.0f);
    } else moveBlend_ = std::max(0.0f, moveBlend_ - dt * 6.0f);

    for (HuntOpponent& o : opps_) updateOpponent(o, dt);

    // keep opponents from overlapping each other
    for (size_t i = 0; i < opps_.size(); ++i)
        for (size_t j = i + 1; j < opps_.size(); ++j) {
            if (opps_[i].defeated || opps_[j].defeated) continue;
            Vec3 d = opps_[j].pos - opps_[i].pos; d.y = 0;
            float l = d.length();
            if (l < 0.8f && l > 0.001f) { Vec3 push = d / l * ((0.8f - l) * 0.5f); opps_[i].pos -= push; opps_[j].pos += push; }
        }

    senseRefresh_ -= dt;
    if (senseRefresh_ <= 0) { updateSense(); senseRefresh_ = 0.25f; }
    float rate = 0.9f + 3.2f * senseLevel_ * senseLevel_;
    heartTimer_ += dt * rate;
    if (heartTimer_ >= 1.0f) heartTimer_ -= 1.0f;
    heartPulse_ = std::max(0.0f, 1.0f - heartTimer_ * 3.0f);

    updateCamera(dt, pad);

    // engagement
    engageCandidate_ = -1; nearestVisible_ = -1;
    float bestD = 1e9f;
    for (int i = 0; i < (int)opps_.size(); ++i) {
        const HuntOpponent& o = opps_[i];
        if (o.defeated) continue;
        float d = distance(o.pos, playerPos_);
        if (d < bestD && d < 12.0f && maze_.lineOfSight(o.pos + Vec3(0, 1, 0), playerPos_ + Vec3(0, 1, 0))) { bestD = d; nearestVisible_ = i; }
    }
    if (nearestVisible_ >= 0 && bestD < 2.8f) engageCandidate_ = nearestVisible_;
    if (!engageAllowed) return -1;
    if (engageCandidate_ >= 0) {
        if (bestD < 1.25f) return engageCandidate_;
        if (pad.justPressed(BTN_START) || pad.justPressed(BTN_LIGHT) || pad.justPressed(BTN_MEDIUM) || pad.justPressed(BTN_HEAVY)) return engageCandidate_;
    }
    return -1;
}

void Hunt::render(Renderer& r, RigRenderer& rig, float time) {
    const auto& ros = roster();
    r.setDirectionalLight(Vec3(-0.35f, -1.0f, -0.25f), Vec3(0.30f, 0.28f, 0.34f), 0.22f);
    r.setShadowFocus(playerPos_, 14.0f);
    r.setFog(Vec3(0.02f, 0.015f, 0.03f), 0.0032f);

    // nearest lights only (16 max)
    struct L { float d; Vec3 p; Vec3 c; float rad; };
    std::vector<L> lights;
    for (const Vec3& t : maze_.torches()) {
        float flick = 0.85f + 0.15f * std::sin(time * 9.0f + t.x * 3.1f + t.z * 1.7f);
        lights.push_back({distance(t, playerPos_), t, Vec3(1.0f, 0.55f, 0.22f) * flick, 7.5f});
    }
    for (const HuntOpponent& o : opps_) {
        if (o.defeated) continue;
        const Character& c = ros[o.charId];
        float pulse = 0.8f + 0.2f * std::sin(time * 5.0f);
        lights.push_back({distance(o.pos, playerPos_) * 0.5f, o.pos + Vec3(0, 1.6f, 0), c.styleColor.rgb() * (1.4f * pulse), 9.0f});
    }
    lights.push_back({0, playerPos_ + Vec3(0, 2.0f, 0), ros[playerChar_].styleColor.rgb() * 0.9f + Vec3(0.3f, 0.3f, 0.3f), 7.0f});
    std::sort(lights.begin(), lights.end(), [](const L& a, const L& b) { return a.d < b.d; });
    r.clearPointLights();
    for (size_t i = 0; i < lights.size() && i < 16; ++i) r.addPointLight(lights[i].p, lights[i].c, lights[i].rad);

    r.draw(floor_, Mat4::identity(), Color(1, 1, 1), 0, 1);
    r.draw(walls_, Mat4::identity(), Color(1, 1, 1), 0, 2);
    r.draw(ceiling_, Mat4::identity(), Color(1, 1, 1), 0, 3, false);
    r.draw(props_, Mat4::identity(), Color(1, 1, 1), 0, 0);
    for (const Vec3& t : maze_.torches()) {
        float flick = 0.8f + 0.2f * std::sin(time * 11.0f + t.x * 2.0f);
        r.drawBillboard(t + Vec3(0, 0.15f, 0), 0.5f * flick, Color(1.0f, 0.6f, 0.2f, 0.7f), Blend::Additive, time * 2.0f);
        r.drawBillboard(t + Vec3(0, 0.2f, 0), 0.25f, Color(1.0f, 0.9f, 0.6f, 0.9f), Blend::Additive);
    }

    // player
    {
        const Character& c = ros[playerChar_];
        Pose p = lerpPose(idlePose(time), [&] {
            Pose w = idlePose(time);
            float s = std::sin(walkPhase_);
            w.legPitch[0] = 0.55f * s; w.legPitch[1] = -0.55f * s;
            w.knee[0] = 0.4f + 0.5f * std::max(0.0f, -s); w.knee[1] = 0.4f + 0.5f * std::max(0.0f, s);
            w.armPitch[0] = 0.5f - 0.5f * s; w.armPitch[1] = 0.5f + 0.5f * s; w.elbow[0] = 0.9f; w.elbow[1] = 0.9f;
            w.armOut[0] = 0.15f; w.armOut[1] = 0.15f;
            w.spinePitch = running_ ? 0.3f : 0.12f; w.spineYaw = 0.1f * s; w.headYaw = 0;
            w.hipHeight = 0.86f + 0.03f * std::fabs(std::cos(walkPhase_));
            return w;
        }(), moveBlend_);
        if (moveBlend_ < 0.5f) { p.spineYaw = 0; p.headYaw = 0; p.armPitch[0] = 0.25f; p.armPitch[1] = 0.25f; p.elbow[0] = 0.5f; p.elbow[1] = 0.5f; }
        rig.draw(r, p, playerPos_, playerYaw_, c, 1.0f);
    }
    // opponents and their Stands
    for (const HuntOpponent& o : opps_) {
        if (o.defeated) continue;
        const Character& c = ros[o.charId];
        Pose p;
        if (o.moving) {
            p = idlePose(time);
            float s = std::sin(o.walkPhase);
            p.legPitch[0] = 0.55f * s; p.legPitch[1] = -0.55f * s;
            p.knee[0] = 0.4f + 0.5f * std::max(0.0f, -s); p.knee[1] = 0.4f + 0.5f * std::max(0.0f, s);
            p.armPitch[0] = 0.5f - 0.5f * s; p.armPitch[1] = 0.5f + 0.5f * s; p.elbow[0] = 0.9f; p.elbow[1] = 0.9f;
            p.spineYaw = 0; p.headYaw = 0; p.spinePitch = o.alert > 0.5f ? 0.3f : 0.1f;
        } else {
            p = idlePose(time + o.charId);
            p.spineYaw = 0; p.headYaw = 0;
            if (o.alert < 0.5f) { p.armPitch[0] = 0.3f; p.armPitch[1] = 0.3f; p.elbow[0] = 0.6f; p.elbow[1] = 0.6f; }
        }
        rig.draw(r, p, o.pos, o.yaw, c, 1.0f);
        if (c.style == StyleType::Stand) {
            Vec3 back(-std::sin(o.yaw), 0, -std::cos(o.yaw));
            Vec3 sp = o.pos + back * 0.7f + Vec3(0, 0.25f + 0.08f * std::sin(time * 2.0f + o.charId), 0);
            rig.draw(r, standIdlePose(time + o.charId), sp, o.yaw, c, 1.05f, 0.45f, 0, true, false);
            r.drawBillboard(sp + Vec3(0, 1.2f, 0), 2.2f, c.styleColor.withAlpha(0.18f), Blend::Additive, time);
        } else {
            r.drawBillboard(o.pos + Vec3(0, 1.0f, 0), 2.4f, c.styleColor.withAlpha(0.14f), Blend::Additive, time * 0.5f);
        }
        // alert marker
        if (o.alert > 0.5f) r.drawBillboard(o.pos + Vec3(0, 2.35f + 0.1f * std::sin(time * 8.0f), 0), 0.35f, Color(1, 0.2f, 0.2f, 0.9f), Blend::Additive);
    }
}

} // namespace dc
