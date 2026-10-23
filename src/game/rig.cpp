#include "game/rig.h"
#include <cmath>

namespace dc {

namespace {
float easeOut(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t); }
float easeInOut(float t) { return smoothstep(0, 1, t); }
}

Pose lerpPose(const Pose& a, const Pose& b, float t) {
    t = clampf(t, 0, 1);
    Pose p;
    p.hipHeight = lerp(a.hipHeight, b.hipHeight, t);
    p.rootOffset = lerp(a.rootOffset, b.rootOffset, t);
    p.rootPitch = lerp(a.rootPitch, b.rootPitch, t);
    p.rootRoll = lerp(a.rootRoll, b.rootRoll, t);
    p.bodyYaw = lerp(a.bodyYaw, b.bodyYaw, t);
    p.spinePitch = lerp(a.spinePitch, b.spinePitch, t);
    p.spineYaw = lerp(a.spineYaw, b.spineYaw, t);
    p.spineRoll = lerp(a.spineRoll, b.spineRoll, t);
    p.headPitch = lerp(a.headPitch, b.headPitch, t);
    p.headYaw = lerp(a.headYaw, b.headYaw, t);
    for (int i = 0; i < 2; ++i) {
        p.armPitch[i] = lerp(a.armPitch[i], b.armPitch[i], t);
        p.armOut[i] = lerp(a.armOut[i], b.armOut[i], t);
        p.elbow[i] = lerp(a.elbow[i], b.elbow[i], t);
        p.legPitch[i] = lerp(a.legPitch[i], b.legPitch[i], t);
        p.legOut[i] = lerp(a.legOut[i], b.legOut[i], t);
        p.knee[i] = lerp(a.knee[i], b.knee[i], t);
    }
    return p;
}

Pose idlePose(float time) {
    Pose p;
    float b = std::sin(time * 2.2f);
    p.hipHeight = 0.86f + 0.015f * b;
    p.spinePitch = 0.08f + 0.02f * b;
    p.spineYaw = -0.35f;
    p.headYaw = 0.3f;
    p.armPitch[0] = 1.1f + 0.05f * b; p.armPitch[1] = 0.8f + 0.05f * b;
    p.armOut[0] = 0.2f; p.armOut[1] = 0.35f;
    p.elbow[0] = 2.0f; p.elbow[1] = 1.9f;
    p.legPitch[0] = 0.25f; p.legPitch[1] = -0.15f;
    p.legOut[0] = 0.18f; p.legOut[1] = 0.18f;
    p.knee[0] = 0.35f; p.knee[1] = 0.3f;
    return p;
}

Pose standIdlePose(float time) {
    Pose p = idlePose(time);
    p.hipHeight = 1.05f + 0.06f * std::sin(time * 1.7f);
    p.spinePitch = 0.05f;
    p.armPitch[0] = 1.2f; p.armPitch[1] = 1.2f;
    p.armOut[0] = -0.1f; p.armOut[1] = -0.1f;
    p.elbow[0] = 2.4f; p.elbow[1] = 2.4f;
    p.legPitch[0] = 0.12f; p.legPitch[1] = 0.2f;
    p.knee[0] = 0.45f; p.knee[1] = 0.55f;
    p.legOut[0] = 0.12f; p.legOut[1] = 0.12f;
    return p;
}

static Pose walkPose(float time, float dir) {
    Pose p = idlePose(time);
    float s = std::sin(time * 9.0f) * dir;
    p.legPitch[0] = 0.5f * s; p.legPitch[1] = -0.5f * s;
    p.knee[0] = 0.4f + 0.4f * std::max(0.0f, -s); p.knee[1] = 0.4f + 0.4f * std::max(0.0f, s);
    p.hipHeight = 0.86f + 0.025f * std::fabs(std::cos(time * 9.0f));
    p.spinePitch = 0.12f;
    return p;
}

static Pose crouchPose(float time) {
    Pose p = idlePose(time);
    p.hipHeight = 0.52f;
    p.spinePitch = 0.45f;
    p.legPitch[0] = 1.5f; p.legPitch[1] = 1.3f;
    p.knee[0] = 2.1f; p.knee[1] = 2.0f;
    p.legOut[0] = 0.3f; p.legOut[1] = 0.3f;
    p.armPitch[0] = 1.3f; p.armPitch[1] = 1.0f;
    p.elbow[0] = 2.2f; p.elbow[1] = 2.1f;
    p.headPitch = -0.3f;
    return p;
}

static Pose jumpPose(float vy) {
    Pose p = idlePose(0);
    float up = clampf(vy * 5.0f, -1, 1);
    p.legPitch[0] = 0.9f - 0.3f * up; p.legPitch[1] = 0.4f;
    p.knee[0] = 1.6f; p.knee[1] = 1.0f;
    p.armPitch[0] = 1.4f; p.armPitch[1] = 1.6f + 0.4f * up;
    p.elbow[0] = 1.6f; p.elbow[1] = 1.2f;
    p.spinePitch = 0.15f;
    return p;
}

static Pose hitPose(float t, bool crouch) {
    Pose p = crouch ? crouchPose(0) : idlePose(0);
    float k = 1.0f - easeOut(t);
    p.spinePitch += -0.45f * k;
    p.headPitch += -0.5f * k;
    p.rootOffset.z = -0.15f * k;
    p.armPitch[0] = 0.5f + 0.4f * k; p.armPitch[1] = 0.3f + 0.6f * k;
    p.armOut[0] = 0.6f * k + 0.2f; p.armOut[1] = 0.7f * k + 0.2f;
    p.elbow[0] = 1.2f; p.elbow[1] = 1.0f;
    if (!crouch) { p.knee[0] = 0.5f; p.knee[1] = 0.5f; p.hipHeight = 0.84f; }
    return p;
}

static Pose airHitPose(float time) {
    Pose p;
    p.hipHeight = 0.9f;
    p.rootPitch = -0.9f;
    p.spinePitch = -0.5f;
    p.headPitch = -0.6f;
    p.armPitch[0] = 2.2f; p.armPitch[1] = 2.4f;
    p.armOut[0] = 0.9f; p.armOut[1] = 0.8f;
    p.elbow[0] = 0.6f; p.elbow[1] = 0.8f;
    p.legPitch[0] = 0.6f; p.legPitch[1] = -0.2f;
    p.knee[0] = 1.2f; p.knee[1] = 0.6f;
    p.legOut[0] = 0.4f; p.legOut[1] = 0.3f;
    return p;
}

static Pose knockdownPose() {
    Pose p;
    p.hipHeight = 0.22f;
    p.rootPitch = -PI * 0.5f;
    p.spinePitch = -0.1f;
    p.headPitch = 0.3f;
    p.armPitch[0] = 0.5f; p.armPitch[1] = 0.3f;
    p.armOut[0] = 1.2f; p.armOut[1] = 0.9f;
    p.elbow[0] = 0.3f; p.elbow[1] = 0.5f;
    p.legPitch[0] = 0.25f; p.legPitch[1] = 0.1f;
    p.knee[0] = 0.5f; p.knee[1] = 0.2f;
    p.legOut[0] = 0.3f; p.legOut[1] = 0.25f;
    return p;
}

static Pose blockPose(bool crouch) {
    Pose p = crouch ? crouchPose(0) : idlePose(0);
    p.armPitch[0] = 1.5f; p.armPitch[1] = 1.3f;
    p.armOut[0] = -0.1f; p.armOut[1] = 0.0f;
    p.elbow[0] = 2.3f; p.elbow[1] = 2.4f;
    p.spinePitch += -0.1f;
    p.headPitch = -0.15f;
    return p;
}

static Pose winPose(float t) {
    Pose p = idlePose(t * 3.0f);
    float k = easeOut(t * 2.0f);
    p.armPitch[1] = lerp(0.8f, 2.9f, k); p.elbow[1] = lerp(1.9f, 0.3f, k); p.armOut[1] = 0.3f;
    p.armPitch[0] = 0.3f; p.elbow[0] = 0.4f; p.armOut[0] = 0.3f;
    p.spinePitch = -0.15f * k; p.headPitch = 0.2f * k;
    return p;
}

static Pose koPose(const Fighter& f) {
    if (f.grounded()) return knockdownPose();
    return airHitPose(0);
}

static Pose styleBreakPose(float time) {
    Pose p = idlePose(0);
    p.spinePitch = 0.55f + 0.08f * std::sin(time * 6.0f);
    p.spineRoll = 0.15f * std::sin(time * 4.0f);
    p.headPitch = 0.4f;
    p.armPitch[0] = 0.2f; p.armPitch[1] = 0.15f;
    p.elbow[0] = 0.3f; p.elbow[1] = 0.35f;
    p.armOut[0] = 0.35f; p.armOut[1] = 0.4f;
    p.hipHeight = 0.78f;
    p.knee[0] = 0.8f; p.knee[1] = 0.7f;
    p.legPitch[0] = 0.5f; p.legPitch[1] = 0.3f;
    return p;
}

static Pose thrownPose(float t) {
    Pose p = idlePose(0);
    p.rootPitch = -0.3f;
    p.armPitch[0] = 2.0f; p.armPitch[1] = 1.9f; p.elbow[0] = 0.8f; p.elbow[1] = 0.7f; p.armOut[0] = 0.6f; p.armOut[1] = 0.6f;
    p.legPitch[0] = 0.4f + 0.3f * std::sin(t * 20.0f); p.legPitch[1] = 0.2f - 0.3f * std::sin(t * 20.0f);
    p.knee[0] = 1.0f; p.knee[1] = 0.9f;
    return p;
}

static Pose rushPose(float time, bool stand) {
    Pose p = idlePose(0);
    float s = std::sin(time * 42.0f);
    p.spinePitch = 0.3f;
    p.spineYaw = 0.25f * s;
    p.armPitch[0] = 1.55f + 0.15f * s; p.armPitch[1] = 1.55f - 0.15f * s;
    p.elbow[0] = s > 0 ? 0.15f : 2.1f; p.elbow[1] = s > 0 ? 2.1f : 0.15f;
    p.armOut[0] = 0.0f; p.armOut[1] = 0.0f;
    p.legPitch[0] = 0.5f; p.legPitch[1] = -0.3f;
    p.knee[0] = 0.7f; p.knee[1] = 0.4f;
    p.hipHeight = stand ? 1.0f : 0.84f;
    return p;
}

Pose attackPose(AnimKind kind, float phase, float time, bool variant) {
    Pose N = idlePose(time);
    Pose W = N, S = N;   // windup, strike
    int a = variant ? 0 : 1;   // acting arm/leg: right by default
    int b = 1 - a;
    switch (kind) {
    case AnimKind::Jab:
        W.armPitch[a] = 1.25f; W.elbow[a] = 2.3f; W.spineYaw = -0.4f;
        S.armPitch[a] = 1.6f; S.elbow[a] = 0.1f; S.armOut[a] = 0.0f; S.spineYaw = 0.35f; S.spinePitch = 0.15f; S.rootOffset.z = 0.08f;
        break;
    case AnimKind::StraightR:
    case AnimKind::StraightL:
        a = kind == AnimKind::StraightL ? 0 : 1; b = 1 - a;
        W.armPitch[a] = 1.0f; W.elbow[a] = 2.5f; W.spineYaw = -0.6f; W.rootOffset.z = -0.08f;
        S.armPitch[a] = 1.6f; S.elbow[a] = 0.0f; S.armOut[a] = 0.0f; S.spineYaw = 0.7f; S.spinePitch = 0.25f; S.rootOffset.z = 0.18f;
        S.legPitch[a] = -0.4f; S.legPitch[b] = 0.6f; S.knee[b] = 0.8f; S.hipHeight = 0.82f;
        break;
    case AnimKind::Hook:
        W.armPitch[a] = 1.2f; W.armOut[a] = 1.1f; W.elbow[a] = 1.6f; W.spineYaw = -0.7f;
        S.armPitch[a] = 1.5f; S.armOut[a] = -0.2f; S.elbow[a] = 1.5f; S.spineYaw = 0.8f; S.spinePitch = 0.2f;
        break;
    case AnimKind::KickR:
    case AnimKind::KickL:
        a = kind == AnimKind::KickL ? 0 : 1; b = 1 - a;
        W.legPitch[a] = 1.3f; W.knee[a] = 2.3f; W.spinePitch = -0.1f; W.hipHeight = 0.9f; W.knee[b] = 0.4f;
        S.legPitch[a] = 1.65f; S.knee[a] = 0.05f; S.spinePitch = -0.35f; S.hipHeight = 0.95f; S.knee[b] = 0.3f;
        S.armPitch[a] = 0.4f; S.armPitch[b] = 1.4f; S.elbow[a] = 0.9f; S.spineYaw = 0.3f;
        break;
    case AnimKind::KneeStrike:
        W.legPitch[a] = 0.9f; W.knee[a] = 1.8f; W.spinePitch = 0.2f;
        S.legPitch[a] = 2.0f; S.knee[a] = 2.6f; S.spinePitch = -0.25f; S.hipHeight = 1.05f;
        S.armPitch[0] = 0.3f; S.armPitch[1] = 0.3f; S.elbow[0] = 1.5f; S.elbow[1] = 1.5f;
        break;
    case AnimKind::Uppercut:
        W.hipHeight = 0.62f; W.spinePitch = 0.5f; W.armPitch[a] = -0.4f; W.elbow[a] = 1.2f; W.knee[0] = 1.6f; W.knee[1] = 1.6f; W.legPitch[0] = 1.1f; W.legPitch[1] = 1.0f;
        S.hipHeight = 1.0f; S.spinePitch = -0.35f; S.armPitch[a] = 3.0f; S.elbow[a] = 0.5f; S.armOut[a] = 0.1f;
        S.legPitch[0] = 0.6f; S.legPitch[1] = 0.1f; S.knee[0] = 1.2f; S.knee[1] = 0.4f; S.armPitch[b] = 0.6f; S.elbow[b] = 1.8f;
        break;
    case AnimKind::Launcher:
        W.hipHeight = 0.7f; W.spinePitch = 0.4f; W.armPitch[0] = 0.0f; W.armPitch[1] = 0.0f; W.elbow[0] = 0.6f; W.elbow[1] = 0.6f;
        S.hipHeight = 0.98f; S.spinePitch = -0.3f; S.armPitch[0] = 2.5f; S.armPitch[1] = 2.6f; S.elbow[0] = 0.3f; S.elbow[1] = 0.3f; S.armOut[0] = 0.15f; S.armOut[1] = 0.15f;
        break;
    case AnimKind::Overhead:
        W.armPitch[a] = 3.1f; W.elbow[a] = 1.0f; W.spinePitch = -0.3f; W.armOut[a] = 0.3f;
        S.armPitch[a] = 0.9f; S.elbow[a] = 0.2f; S.spinePitch = 0.65f; S.headPitch = 0.3f; S.rootOffset.z = 0.12f; S.hipHeight = 0.8f;
        break;
    case AnimKind::Stomp:
        W.legPitch[a] = 1.9f; W.knee[a] = 2.6f; W.hipHeight = 0.95f; W.spinePitch = -0.2f; W.armPitch[0] = 2.2f; W.armPitch[1] = 2.2f; W.elbow[0] = 0.8f; W.elbow[1] = 0.8f;
        S.legPitch[a] = 0.6f; S.knee[a] = 0.1f; S.spinePitch = 0.5f; S.hipHeight = 0.75f; S.rootOffset.z = 0.2f; S.armPitch[0] = 0.4f; S.armPitch[1] = 0.4f;
        break;
    case AnimKind::Sweep:
        W.hipHeight = 0.5f; W.spinePitch = 0.5f; W.legPitch[0] = 1.4f; W.legPitch[1] = 1.4f; W.knee[0] = 2.2f; W.knee[1] = 2.2f;
        S.hipHeight = 0.42f; S.spinePitch = 0.7f; S.bodyYaw = 1.2f; S.legPitch[a] = 1.3f; S.knee[a] = 0.0f; S.legOut[a] = 0.4f;
        S.legPitch[b] = 1.5f; S.knee[b] = 2.4f; S.armPitch[0] = 0.4f; S.armPitch[1] = 0.6f; S.elbow[0] = 0.6f; S.elbow[1] = 0.8f;
        break;
    case AnimKind::LowKick:
        W = crouchPose(time); W.legPitch[a] = 1.1f; W.knee[a] = 2.4f;
        S = crouchPose(time); S.legPitch[a] = 1.1f; S.knee[a] = 0.0f; S.spinePitch = 0.2f; S.hipHeight = 0.55f;
        break;
    case AnimKind::Rush:
        return rushPose(time, false);
    case AnimKind::Cast:
        W.armPitch[0] = 0.6f; W.armPitch[1] = 0.6f; W.elbow[0] = 2.4f; W.elbow[1] = 2.4f; W.spinePitch = -0.1f;
        S.armPitch[0] = 1.55f; S.armPitch[1] = 1.55f; S.elbow[0] = 0.1f; S.elbow[1] = 0.1f; S.armOut[0] = 0.25f; S.armOut[1] = 0.25f; S.spinePitch = 0.2f; S.rootOffset.z = 0.1f;
        break;
    case AnimKind::Grab:
        W.armPitch[0] = 1.4f; W.armPitch[1] = 1.4f; W.elbow[0] = 0.3f; W.elbow[1] = 0.3f; W.armOut[0] = 0.5f; W.armOut[1] = 0.5f; W.spinePitch = 0.3f;
        S.armPitch[0] = 1.3f; S.armPitch[1] = 1.3f; S.elbow[0] = 1.6f; S.elbow[1] = 1.6f; S.armOut[0] = 0.0f; S.armOut[1] = 0.0f; S.spinePitch = -0.2f; S.hipHeight = 0.95f;
        break;
    case AnimKind::DiveKick:
        W.legPitch[a] = 1.2f; W.knee[a] = 2.0f; W.rootPitch = -0.3f;
        S.legPitch[0] = 1.5f; S.legPitch[1] = 1.3f; S.knee[0] = 0.1f; S.knee[1] = 0.6f; S.rootPitch = -0.6f; S.armPitch[0] = 2.0f; S.armPitch[1] = 2.0f; S.elbow[0] = 0.5f; S.elbow[1] = 0.5f;
        break;
    case AnimKind::Slide:
        W.hipHeight = 0.55f; W.spinePitch = 0.4f; W.knee[0] = 2.0f; W.knee[1] = 2.0f; W.legPitch[0] = 1.3f; W.legPitch[1] = 1.3f;
        S.hipHeight = 0.3f; S.rootPitch = -1.0f; S.spinePitch = 0.5f; S.legPitch[a] = 0.9f; S.knee[a] = 0.1f; S.legPitch[b] = 1.4f; S.knee[b] = 2.2f;
        S.armPitch[0] = 0.0f; S.armPitch[1] = 2.2f; S.elbow[0] = 0.4f; S.elbow[1] = 0.6f; S.armOut[0] = 1.0f;
        break;
    case AnimKind::Taunt: {
        Pose T = idlePose(time);
        T.armPitch[1] = 1.4f; T.elbow[1] = 2.0f; T.armOut[1] = 0.6f + 0.25f * std::sin(time * 10.0f);
        T.armPitch[0] = 0.0f; T.elbow[0] = 1.9f; T.armOut[0] = 0.5f;
        T.spinePitch = -0.15f; T.headPitch = 0.25f; T.headYaw = 0.5f; T.spineYaw = 0.2f;
        T.legPitch[0] = -0.2f; T.legPitch[1] = 0.4f; T.knee[1] = 0.6f;
        float t = phase / 3.0f;
        float k = std::sin(PI * clampf(t, 0, 1));
        return lerpPose(N, T, easeOut(k * 2.0f));
    }
    case AnimKind::Spin:
        W.armOut[0] = 0.3f; W.armOut[1] = 0.3f; W.hipHeight = 0.7f; W.knee[0] = 1.4f; W.knee[1] = 1.4f; W.legPitch[0] = 0.8f; W.legPitch[1] = 0.8f;
        S.armPitch[0] = 1.5f; S.armPitch[1] = 1.5f; S.armOut[0] = 1.5f; S.armOut[1] = 1.5f; S.elbow[0] = 0.1f; S.elbow[1] = 0.1f;
        S.hipHeight = 1.0f; S.bodyYaw = 2.0f * PI; S.legPitch[0] = 0.5f; S.legPitch[1] = 0.5f; S.knee[0] = 1.0f; S.knee[1] = 1.0f;
        break;
    case AnimKind::Headbutt:
        W.spinePitch = -0.5f; W.headPitch = -0.5f; W.hipHeight = 0.75f; W.knee[0] = 1.0f; W.knee[1] = 1.0f;
        S.spinePitch = 0.8f; S.headPitch = 0.4f; S.hipHeight = 0.9f; S.rootOffset.z = 0.25f; S.armPitch[0] = -0.3f; S.armPitch[1] = -0.3f; S.elbow[0] = 0.4f; S.elbow[1] = 0.4f;
        break;
    case AnimKind::Lariat:
        W.armPitch[a] = 0.4f; W.armOut[a] = 1.7f; W.elbow[a] = 0.3f; W.spineYaw = -0.9f; W.spinePitch = 0.2f;
        S.armPitch[a] = 1.5f; S.armOut[a] = 1.5f; S.elbow[a] = 0.15f; S.spineYaw = 0.9f; S.spinePitch = 0.3f; S.rootOffset.z = 0.15f; S.legPitch[b] = 0.7f; S.knee[b] = 0.9f;
        break;
    }
    if (phase < 1.0f) return lerpPose(N, W, easeInOut(phase));
    if (phase < 2.0f) return lerpPose(W, S, easeOut((phase - 1.0f) * 2.5f));
    return lerpPose(S, N, easeInOut(phase - 2.0f));
}

Pose poseForFighter(const Fighter& f, float time) {
    switch (f.state) {
    case FState::Intro: return idlePose(time);
    case FState::Idle: return idlePose(time);
    case FState::Walk: return walkPose(time, 1.0f);
    case FState::Crouch: return crouchPose(time);
    case FState::Jump: return jumpPose(f.vy);
    case FState::Land: return lerpPose(crouchPose(time), idlePose(time), (float)f.timer / 4.0f);
    case FState::Dash: {
        Pose p = walkPose(time * 1.5f, 1.0f); p.spinePitch = 0.4f; p.rootOffset.z = 0.1f; return p;
    }
    case FState::Backdash: {
        Pose p = jumpPose(0.0f); p.spinePitch = -0.3f; p.hipHeight = 0.95f; return p;
    }
    case FState::Sidestep: {
        float t = (float)f.timer / (float)std::max(1, f.stateLen);
        Pose p = idlePose(time); p.bodyYaw = (f.sideTarget < 0 ? 1.0f : -1.0f) * std::sin(PI * t) * 0.9f;
        p.hipHeight = 0.86f + 0.12f * std::sin(PI * t); p.knee[0] = 0.9f; p.knee[1] = 0.9f; p.legPitch[0] = 0.5f; p.legPitch[1] = 0.5f;
        return p;
    }
    case FState::Attack: {
        const Move* m = f.move;
        if (!m) return idlePose(time);
        float ph;
        if (f.moveFrame < m->startup) ph = (float)f.moveFrame / (float)std::max(1, m->startup);
        else if (f.moveFrame < m->startup + m->active) ph = 1.0f + (float)(f.moveFrame - m->startup) / (float)std::max(1, m->active);
        else ph = 2.0f + (float)(f.moveFrame - m->startup - m->active) / (float)std::max(1, m->recovery);
        if (m->standAttack) {
            // the user points / commands while the Stand attacks
            Pose p = idlePose(time);
            float k = std::min(1.0f, ph / 0.8f) * (ph > 2.3f ? std::max(0.0f, 1.0f - (ph - 2.3f) / 0.7f) : 1.0f);
            Pose cmd = p; cmd.armPitch[1] = 1.6f; cmd.elbow[1] = 0.1f; cmd.armOut[1] = 0.1f; cmd.spinePitch = 0.15f; cmd.spineYaw = 0.4f;
            cmd.legPitch[0] = 0.5f; cmd.legPitch[1] = -0.3f; cmd.knee[0] = 0.6f; cmd.hipHeight = 0.84f;
            return lerpPose(p, cmd, easeOut(k));
        }
        bool variant = (m->chainLevel == 0 && m->stance == Stance::Crouch);
        Pose p = attackPose(m->anim, ph, time, variant);
        if (m->stance == Stance::Air) { Pose j = jumpPose(f.vy); p.legPitch[0] = lerp(p.legPitch[0], j.legPitch[0], 0.5f); }
        if (m->stance == Stance::Crouch && m->anim != AnimKind::LowKick && m->anim != AnimKind::Sweep) {
            Pose c = crouchPose(time); p.hipHeight = c.hipHeight; p.legPitch[0] = c.legPitch[0]; p.legPitch[1] = c.legPitch[1]; p.knee[0] = c.knee[0]; p.knee[1] = c.knee[1];
            p.spinePitch += 0.3f;
        }
        return p;
    }
    case FState::Block: return blockPose(false);
    case FState::CrouchBlock: return blockPose(true);
    case FState::Hit: return hitPose((float)f.timer / (float)std::max(1, f.stateLen), false);
    case FState::CrouchHit: return hitPose((float)f.timer / (float)std::max(1, f.stateLen), true);
    case FState::AirHit: return airHitPose(time);
    case FState::Knockdown: {
        float t = (float)f.timer / 8.0f;
        return lerpPose(airHitPose(time), knockdownPose(), easeOut(t));
    }
    case FState::GetUp: return lerpPose(knockdownPose(), idlePose(time), easeInOut((float)f.timer / (float)std::max(1, f.stateLen)));
    case FState::Thrown: return thrownPose(time);
    case FState::StyleBreak: return styleBreakPose(time);
    case FState::Clash: return rushPose(time, false);
    case FState::KO: return koPose(f);
    case FState::Win: return winPose((float)f.timer / 60.0f);
    case FState::Taunt: return attackPose(AnimKind::Taunt, 1.5f, time, false);
    }
    return idlePose(time);
}

// --------------------------------------------------------------- renderer ----
bool RigRenderer::init() {
    MeshBuilder b;
    b.addBox(Vec3(0, 0, 0), Vec3(1, 1, 1), Vec3(1, 1, 1));
    box_.upload(b);
    b.clear();
    b.addSphere(Vec3(0, 0, 0), 0.5f, 10, 14, Vec3(1, 1, 1));
    sphere_.upload(b);
    return true;
}

void RigRenderer::draw(Renderer& r, const Pose& p, const Vec3& feet, float yaw, const Character& c, float scale,
                       float alpha, float flash, bool asStand, bool castShadow) {
    const Vec3 skinV(0.92f, 0.74f, 0.62f);
    Color body = asStand ? c.styleColor : c.bodyColor;
    Color accent = asStand ? mix(c.styleColor, Color(1, 1, 1), 0.5f) : c.accentColor;
    Color skin = asStand ? mix(c.styleColor, Color(1, 1, 1), 0.3f) : Color(skinV.x, skinV.y, skinV.z);
    Color hair = asStand ? mix(c.styleColor, Color(1, 1, 1), 0.7f) : c.hairColor;
    Color dark = asStand ? c.styleColor * 0.8f : c.bodyColor * 0.6f;
    float em = asStand ? 0.55f : 0.0f;
    if (flash > 0) {
        body = mix(body, Color(1, 1, 1), flash); accent = mix(accent, Color(1, 1, 1), flash);
        skin = mix(skin, Color(1, 1, 1), flash); hair = mix(hair, Color(1, 1, 1), flash); dark = mix(dark, Color(1, 1, 1), flash);
        em = std::max(em, flash * 0.8f);
    }
    auto A = [&](Color col) { col.a = alpha; return col; };

    float heightScale = (c.height / 1.85f) * scale;
    Mat4 root = Mat4::translate(feet) * Mat4::rotateY(yaw + p.bodyYaw) * Mat4::scale(Vec3(heightScale))
              * Mat4::translate(Vec3(0, p.hipHeight, 0) + p.rootOffset) * Mat4::rotateX(p.rootPitch) * Mat4::rotateZ(p.rootRoll);

    auto drawBox = [&](const Mat4& m, Vec3 center, Vec3 size, Color col) {
        r.draw(box_, m * Mat4::translate(center) * Mat4::scale(size), A(col), em, 0, castShadow);
    };
    auto drawSphere = [&](const Mat4& m, Vec3 center, float radius, Color col) {
        r.draw(sphere_, m * Mat4::translate(center) * Mat4::scale(Vec3(radius * 2)), A(col), em, 0, castShadow);
    };

    // pelvis
    drawBox(root, Vec3(0, 0.0f, 0), Vec3(0.34f, 0.2f, 0.22f), dark);
    // torso
    Mat4 spine = root * Mat4::translate(Vec3(0, 0.1f, 0)) * Mat4::rotateX(p.spinePitch) * Mat4::rotateY(p.spineYaw) * Mat4::rotateZ(p.spineRoll);
    drawBox(spine, Vec3(0, 0.24f, 0), Vec3(0.40f, 0.48f, 0.24f), body);
    drawBox(spine, Vec3(0, 0.40f, 0.01f), Vec3(0.46f, 0.14f, 0.27f), accent);  // shoulders / collar
    // head
    Mat4 neck = spine * Mat4::translate(Vec3(0, 0.5f, 0)) * Mat4::rotateX(p.headPitch) * Mat4::rotateY(p.headYaw);
    drawBox(neck, Vec3(0, 0.03f, 0), Vec3(0.1f, 0.08f, 0.1f), skin);
    drawSphere(neck, Vec3(0, 0.19f, 0), 0.135f, skin);
    drawBox(neck, Vec3(0, 0.27f, -0.02f), Vec3(0.26f, 0.12f, 0.26f), hair);
    drawBox(neck, Vec3(0, 0.19f, -0.11f), Vec3(0.24f, 0.2f, 0.08f), hair);
    // eyes
    if (!asStand) {
        drawBox(neck, Vec3(0.05f, 0.2f, 0.125f), Vec3(0.04f, 0.03f, 0.02f), Color(0.05f, 0.05f, 0.08f));
        drawBox(neck, Vec3(-0.05f, 0.2f, 0.125f), Vec3(0.04f, 0.03f, 0.02f), Color(0.05f, 0.05f, 0.08f));
    }

    const float sideX[2] = {1.0f, -1.0f};   // left, right (local +x is the character's left when facing +z)
    for (int i = 0; i < 2; ++i) {
        float sx = sideX[i];
        // arm
        Mat4 shoulder = spine * Mat4::translate(Vec3(sx * 0.25f, 0.42f, 0)) * Mat4::rotateZ(sx * p.armOut[i]) * Mat4::rotateX(-p.armPitch[i]);
        drawBox(shoulder, Vec3(0, -0.15f, 0), Vec3(0.13f, 0.3f, 0.13f), body);
        Mat4 elbow = shoulder * Mat4::translate(Vec3(0, -0.3f, 0)) * Mat4::rotateX(-p.elbow[i]);
        drawBox(elbow, Vec3(0, -0.14f, 0), Vec3(0.11f, 0.28f, 0.11f), accent);
        drawBox(elbow, Vec3(0, -0.32f, 0.01f), Vec3(0.12f, 0.12f, 0.12f), skin);
        // leg
        Mat4 hip = root * Mat4::translate(Vec3(sx * 0.11f, -0.08f, 0)) * Mat4::rotateZ(sx * p.legOut[i]) * Mat4::rotateX(-p.legPitch[i]);
        drawBox(hip, Vec3(0, -0.22f, 0), Vec3(0.16f, 0.45f, 0.17f), dark);
        Mat4 knee = hip * Mat4::translate(Vec3(0, -0.45f, 0)) * Mat4::rotateX(p.knee[i]);
        drawBox(knee, Vec3(0, -0.22f, 0), Vec3(0.14f, 0.45f, 0.15f), body);
        drawBox(knee, Vec3(0, -0.47f, 0.06f), Vec3(0.14f, 0.09f, 0.28f), accent);
    }
}

} // namespace dc
