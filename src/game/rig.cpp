#include "game/rig.h"
#include <cmath>

namespace dc {

namespace {
float easeOut(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t); }
float easeInOut(float t) { return smoothstep(0, 1, t); }

float boxBottom(const Mat4& model) {
    // A unit box spans [-0.5, 0.5] on every axis. Project its half extents onto world Y.
    return model.m[3][1] - 0.5f * (std::fabs(model.m[0][1]) + std::fabs(model.m[1][1]) + std::fabs(model.m[2][1]));
}
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
        Pose p = idlePose(time); p.bodyYaw = -f.sideDir * std::sin(PI * t) * 0.9f;
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

std::array<RigLeg, 2> rigLegs(const Pose& p, const Mat4& root) {
    std::array<RigLeg, 2> legs;
    for (int i = 0; i < 2; ++i) {
        const float side = i == 0 ? 1.0f : -1.0f;
        const Mat4 hip = root * Mat4::translate(Vec3(side * 0.11f, -0.08f, 0))
                       * Mat4::rotateZ(side * p.legOut[i]) * Mat4::rotateX(-p.legPitch[i]);
        const Mat4 knee = hip * Mat4::translate(Vec3(0, -0.45f, 0)) * Mat4::rotateX(p.knee[i]);
        legs[i].thigh = hip * Mat4::translate(Vec3(0, -0.22f, 0)) * Mat4::scale(Vec3(0.16f, 0.45f, 0.17f));
        legs[i].shin = knee * Mat4::translate(Vec3(0, -0.22f, 0)) * Mat4::scale(Vec3(0.14f, 0.45f, 0.15f));
        legs[i].foot = knee * Mat4::translate(Vec3(0, -0.47f, 0.06f)) * Mat4::scale(Vec3(0.14f, 0.09f, 0.28f));
    }
    return legs;
}

Mat4 rigRoot(const Pose& p, const Vec3& feet, float yaw, float heightScale, bool asStand, float floorY) {
    Mat4 root = Mat4::translate(feet) * Mat4::rotateY(yaw + p.bodyYaw) * Mat4::scale(Vec3(heightScale))
              * Mat4::translate(Vec3(0, p.hipHeight, 0) + p.rootOffset)
              * Mat4::rotateX(p.rootPitch) * Mat4::rotateZ(p.rootRoll);
    if (!asStand) {
        // Correct only actual penetration. Preserve clear/airborne poses and simulation positions.
        // Include thighs and shins: during sweeps or knockdowns a foot need not be the lowest part.
        float bottom = floorY;
        for (const RigLeg& leg : rigLegs(p, root)) {
            bottom = std::min(bottom, boxBottom(leg.thigh));
            bottom = std::min(bottom, boxBottom(leg.shin));
            bottom = std::min(bottom, boxBottom(leg.foot));
        }
        root.m[3][1] += floorY - bottom;
    }
    return root;
}

// --------------------------------------------------------------- renderer ----
namespace {

// Orientation whose +Y axis points along dir, positioned at pos (for spikes, horns, ribbons).
Mat4 aim(const Vec3& pos, const Vec3& dir) {
    Vec3 y = normalize(dir);
    Vec3 ref = std::fabs(y.y) > 0.9f ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
    Vec3 x = normalize(cross(ref, y));
    Vec3 z = cross(x, y);
    Mat4 m;
    m.m[0][0] = x.x; m.m[0][1] = x.y; m.m[0][2] = x.z;
    m.m[1][0] = y.x; m.m[1][1] = y.y; m.m[1][2] = y.z;
    m.m[2][0] = z.x; m.m[2][1] = z.y; m.m[2][2] = z.z;
    m.m[3][0] = pos.x; m.m[3][1] = pos.y; m.m[3][2] = pos.z;
    return m;
}

Color shade(Color c, float k) { return Color(c.r * k, c.g * k, c.b * k, c.a); }

// Thin wrapper that submits unit meshes with the per-draw alpha / emissive / shadow settings.
struct Painter {
    Renderer& r;
    const Mesh *boxMesh, *sphereMesh, *cylMesh, *coneMesh;
    float alpha, em;
    bool shadow;

    void put(const Mesh* mesh, const Mat4& m, Color col, float glow) const {
        col.a = alpha;
        r.draw(*mesh, m, col, std::max(em, glow), 0, shadow);
    }
    // size is the full extent on each axis
    void box(const Mat4& m, Vec3 c, Vec3 s, Color col, float glow = 0) const { put(boxMesh, m * Mat4::translate(c) * Mat4::scale(s), col, glow); }
    void ball(const Mat4& m, Vec3 c, Vec3 s, Color col, float glow = 0) const { put(sphereMesh, m * Mat4::translate(c) * Mat4::scale(s), col, glow); }
    void tube(const Mat4& m, Vec3 c, Vec3 s, Color col, float glow = 0) const { put(cylMesh, m * Mat4::translate(c) * Mat4::scale(s), col, glow); }
    // cone from base towards dir (in m's frame); width = base diameter
    void spike(const Mat4& m, Vec3 base, Vec3 dir, float len, float width, Color col, float glow = 0) const {
        Vec3 d = normalize(dir);
        put(coneMesh, m * aim(base + d * (len * 0.5f), d) * Mat4::scale(Vec3(width, len, width)), col, glow);
    }
};

struct Palette {
    Color body, accent, skin, hair, dark, pants, boots, cloth, eye;
};

// ------------------------------------------------------------------ hair ----
void drawHair(const Painter& P, const Mat4& head, const Look& L, const Palette& pal, float t) {
    const float H = L.head;
    const Color hair = pal.hair;
    const float sway = std::sin(t * 2.4f);
    if (L.hair == HairStyle::Bald) return;
    // cap: sits on the top / back of the skull, leaving the face clear
    P.ball(head, Vec3(0, 0.235f, -0.04f) * H, Vec3(0.295f, 0.25f, 0.31f) * H, hair);
    switch (L.hair) {
    case HairStyle::Spiky: {
        // wild swept-back spikes
        struct S { Vec3 p, d; float len, w; };
        const S spikes[] = {
            {{0.00f, 0.32f, 0.02f}, {0.0f, 1.0f, 0.5f}, 0.20f, 0.10f},
            {{0.07f, 0.31f, -0.03f}, {0.5f, 1.0f, -0.1f}, 0.19f, 0.10f},
            {{-0.07f, 0.31f, -0.03f}, {-0.5f, 1.0f, -0.1f}, 0.19f, 0.10f},
            {{0.10f, 0.28f, -0.09f}, {0.9f, 0.5f, -0.8f}, 0.20f, 0.09f},
            {{-0.10f, 0.28f, -0.09f}, {-0.9f, 0.5f, -0.8f}, 0.20f, 0.09f},
            {{0.00f, 0.29f, -0.12f}, {0.0f, 0.5f, -1.0f}, 0.24f, 0.11f},
            {{0.00f, 0.22f, -0.15f}, {0.0f, -0.1f, -1.0f}, 0.20f, 0.10f},
            {{0.05f, 0.28f, 0.10f}, {0.3f, 0.2f, 1.0f}, 0.12f, 0.06f},
            {{-0.05f, 0.28f, 0.10f}, {-0.3f, 0.2f, 1.0f}, 0.12f, 0.06f},
        };
        for (const S& s : spikes) P.spike(head, s.p * H, s.d, s.len * H, s.w * H, hair);
        break;
    }
    case HairStyle::LongFlow: {
        // curtain down the back, face-framing locks and a side-swept fringe
        Mat4 back = head * Mat4::translate(Vec3(0, 0.30f, -0.13f)) * Mat4::rotateX(0.10f + 0.05f * sway);
        P.box(back, Vec3(0, -0.28f, 0), Vec3(0.27f, 0.56f, 0.06f), hair);
        P.box(back * Mat4::translate(Vec3(0, -0.56f, 0)) * Mat4::rotateX(0.08f * sway), Vec3(0, -0.10f, 0), Vec3(0.21f, 0.22f, 0.05f), shade(hair, 0.92f));
        for (int s = -1; s <= 1; s += 2) {
            P.box(head, Vec3(s * 0.132f, 0.13f, 0.01f), Vec3(0.045f, 0.24f, 0.09f), hair);
            P.box(head * Mat4::rotateZ(-s * 0.18f), Vec3(s * 0.035f, 0.292f, 0.105f), Vec3(0.16f, 0.05f, 0.07f), hair);
        }
        break;
    }
    case HairStyle::Ponytail: {
        // high tie, then a swaying tail made of tapering segments
        P.ball(head, Vec3(0, 0.325f, -0.09f), Vec3(0.09f, 0.08f, 0.09f), pal.accent);
        Mat4 m = head * Mat4::translate(Vec3(0, 0.33f, -0.1f));
        float w = 0.11f;
        for (int i = 0; i < 4; ++i) {
            m = m * Mat4::rotateX(0.55f + 0.14f * sway + (i > 0 ? 0.12f : 0.0f));
            float len = 0.17f;
            P.box(m, Vec3(0, -len * 0.5f, 0), Vec3(w, len, w * 0.85f), i == 3 ? shade(hair, 0.9f) : hair);
            m = m * Mat4::translate(Vec3(0, -len, 0));
            w *= 0.82f;
        }
        for (int s = -1; s <= 1; s += 2) P.box(head, Vec3(s * 0.128f, 0.15f, 0.04f), Vec3(0.04f, 0.2f, 0.08f), hair);
        P.box(head, Vec3(0, 0.30f, 0.108f), Vec3(0.22f, 0.04f, 0.05f), hair);
        break;
    }
    case HairStyle::Mohawk: {
        // blazing ridge of ember-coloured hair
        for (int i = 0; i < 7; ++i) {
            float k = (float)i / 6.0f;
            float z = lerp(0.10f, -0.14f, k);
            float h = 0.13f + 0.07f * std::sin(k * PI);
            P.box(head * Mat4::translate(Vec3(0, 0.285f - 0.09f * k * k + 0.02f * k, z)) * Mat4::rotateX(-0.2f * (k - 0.4f)),
                  Vec3(0, h * 0.5f, 0), Vec3(0.055f, h, 0.075f), mix(hair, Color(1.0f, 0.75f, 0.3f), 0.35f * (1.0f - std::fabs(k - 0.5f) * 2.0f)), 0.35f);
        }
        break;
    }
    case HairStyle::TopKnot: {
        P.ball(head, Vec3(0, 0.375f, -0.03f), Vec3(0.12f, 0.11f, 0.12f), hair);
        P.tube(head, Vec3(0, 0.318f, -0.03f), Vec3(0.07f, 0.03f, 0.07f), pal.accent);
        Mat4 tail = head * Mat4::translate(Vec3(0, 0.22f, -0.14f)) * Mat4::rotateX(0.35f + 0.1f * sway);
        P.box(tail, Vec3(0, -0.12f, 0), Vec3(0.07f, 0.26f, 0.05f), hair);
        for (int s = -1; s <= 1; s += 2) P.box(head, Vec3(s * 0.128f, 0.17f, 0.02f), Vec3(0.035f, 0.14f, 0.07f), hair);
        break;
    }
    default: break;
    }
}

// ------------------------------------------------------------ stand parts ----
void drawStandOrnaments(const Painter& P, const Mat4& spine, const Mat4& head, const Mat4 shoulder[2], const Look& L,
                        const Color& style, float t) {
    const float S = L.shoulders, T = L.torso;
    const Color bright = mix(style, Color(1, 1, 1), 0.65f);
    const float sway = std::sin(t * 2.2f);
    switch (L.stand) {
    case StandDesign::Heart: {
        // a beating core, spiked pauldrons and a crest of horns
        float beat = 0.9f + 0.25f * std::max(0.0f, std::sin(t * 6.0f));
        P.ball(spine, Vec3(0, 0.33f, 0.115f * T), Vec3(0.17f, 0.17f, 0.12f) * beat, bright, 1.0f);
        P.ball(spine, Vec3(0, 0.33f, 0.105f * T), Vec3(0.23f, 0.23f, 0.12f) * beat, style, 0.8f);
        for (int i = 0; i < 2; ++i) {
            float sx = i == 0 ? 1.0f : -1.0f;
            P.ball(shoulder[i], Vec3(sx * 0.03f, 0.04f, 0), Vec3(0.26f, 0.16f, 0.24f), shade(style, 0.7f));
            P.spike(shoulder[i], Vec3(sx * 0.06f, 0.08f, 0), Vec3(sx * 0.8f, 1.0f, 0.1f), 0.22f, 0.09f, bright);
            P.spike(shoulder[i], Vec3(sx * 0.04f, 0.04f, -0.07f), Vec3(sx * 0.5f, 0.5f, -1.0f), 0.17f, 0.07f, bright);
            P.spike(head, Vec3(sx * 0.07f, 0.31f, 0.0f), Vec3(sx * 0.7f, 1.0f, -0.5f), 0.22f, 0.07f, bright);
        }
        P.box(head, Vec3(0, 0.12f, 0.1f), Vec3(0.16f, 0.1f, 0.07f), shade(style, 0.65f));   // mask
        break;
    }
    case StandDesign::Crystal: {
        // faceted shards growing from the shoulders, back and brow
        const Vec3 up(0, 1, 0);
        for (int i = 0; i < 2; ++i) {
            float sx = i == 0 ? 1.0f : -1.0f;
            P.spike(shoulder[i], Vec3(sx * 0.05f, 0.05f, 0), Vec3(sx * 0.5f, 1.0f, -0.1f), 0.34f, 0.11f, bright, 0.5f);
            P.spike(shoulder[i], Vec3(sx * 0.07f, 0.04f, 0.05f), Vec3(sx * 1.0f, 0.5f, 0.3f), 0.22f, 0.08f, style, 0.4f);
            P.spike(shoulder[i], Vec3(sx * 0.03f, 0.04f, -0.06f), Vec3(sx * 0.4f, 0.8f, -1.0f), 0.26f, 0.08f, style, 0.4f);
            P.spike(head, Vec3(sx * 0.06f, 0.3f, 0.02f), Vec3(sx * 0.35f, 1.0f, 0.1f), 0.2f + 0.04f * i, 0.06f, bright, 0.6f);
        }
        P.spike(head, Vec3(0, 0.32f, 0.0f), up, 0.3f, 0.08f, bright, 0.7f);
        P.spike(spine, Vec3(0, 0.45f, -0.1f), Vec3(0, 1.0f, -0.6f), 0.3f, 0.1f, style, 0.4f);
        P.spike(spine, Vec3(0.09f, 0.35f, -0.12f), Vec3(0.6f, 0.7f, -1.0f), 0.24f, 0.08f, style, 0.4f);
        P.spike(spine, Vec3(-0.09f, 0.35f, -0.12f), Vec3(-0.6f, 0.7f, -1.0f), 0.24f, 0.08f, style, 0.4f);
        P.spike(spine, Vec3(0, 0.30f, 0.12f * T), Vec3(0, 0.1f, 1.0f), 0.12f, 0.1f, bright, 0.9f);   // chest prism
        break;
    }
    case StandDesign::Bell: {
        // a cathedral-bell helm, heavy domed shoulders and a clapper
        P.spike(head, Vec3(0, 0.22f, 0), Vec3(0, 1.0f, 0), 0.36f, 0.42f, shade(style, 0.85f));
        P.tube(head, Vec3(0, 0.215f, 0), Vec3(0.46f, 0.05f, 0.46f), bright, 0.3f);
        P.ball(head, Vec3(0, 0.15f, 0.0f), Vec3(0.09f, 0.09f, 0.09f), bright, 0.8f);
        for (int i = 0; i < 2; ++i) {
            float sx = i == 0 ? 1.0f : -1.0f;
            P.ball(shoulder[i], Vec3(sx * 0.05f, 0.04f, 0), Vec3(0.34f, 0.22f, 0.32f), shade(style, 0.8f));
            P.tube(shoulder[i], Vec3(sx * 0.05f, -0.03f, 0), Vec3(0.3f, 0.04f, 0.28f), bright, 0.3f);
        }
        P.box(spine, Vec3(0, 0.33f, 0.14f * T), Vec3(0.34f * T, 0.3f, 0.05f), shade(style, 0.75f));   // breast plate
        P.tube(spine, Vec3(0, 0.33f, 0.17f * T), Vec3(0.12f, 0.02f, 0.12f), bright, 0.9f);
        break;
    }
    case StandDesign::Silk: {
        // trailing ribbons and a halo
        P.tube(head * Mat4::rotateX(PI * 0.5f), Vec3(0, 0.13f, 0.1f), Vec3(0.42f, 0.02f, 0.42f), bright, 0.9f);
        for (int k = 0; k < 4; ++k) {
            float sx = (k & 1) ? 1.0f : -1.0f;
            float off = 0.08f + 0.1f * (k >> 1);
            Mat4 m = spine * Mat4::translate(Vec3(sx * off, 0.42f, -0.12f)) * Mat4::rotateZ(sx * (0.35f + 0.1f * k) + 0.1f * std::sin(t * 2.0f + k))
                   * Mat4::rotateX(0.9f + 0.2f * std::sin(t * 2.6f + k * 1.7f));
            for (int i = 0; i < 3; ++i) {
                P.box(m, Vec3(0, -0.17f, 0), Vec3(0.07f - 0.015f * i, 0.36f, 0.015f), i & 1 ? bright : style, 0.5f);
                m = m * Mat4::translate(Vec3(0, -0.34f, 0)) * Mat4::rotateX(0.35f * std::sin(t * 3.0f + k + i));
            }
        }
        P.ball(spine, Vec3(0, 0.34f, 0.125f * T), Vec3(0.12f, 0.12f, 0.08f), bright, 1.0f);
        (void)S; (void)sway;
        break;
    }
    case StandDesign::None: break;
    }
    // glowing eyes on every Stand
    for (int s = -1; s <= 1; s += 2) P.box(head, Vec3(s * 0.052f, 0.205f, 0.128f * L.head), Vec3(0.06f, 0.022f, 0.02f), Color(1, 1, 1), 1.0f);
}

// --------------------------------------------------------------- clothing ----
void drawFace(const Painter& P, const Mat4& head, const Look& L, const Palette& pal) {
    const float H = L.head;
    const Color dark(0.05f, 0.05f, 0.08f);
    const Color skinDark = shade(pal.skin, 0.8f);
    for (int s = -1; s <= 1; s += 2) {
        P.box(head, Vec3(s * 0.052f, 0.205f, 0.126f * H), Vec3(0.046f, 0.03f, 0.02f), Color(0.96f, 0.96f, 0.98f));   // eye white
        P.box(head, Vec3(s * 0.052f, 0.205f, 0.134f * H), Vec3(0.022f, 0.028f, 0.014f), pal.eye);
        // angled brow gives everyone a determined stare
        P.box(head * Mat4::translate(Vec3(s * 0.056f, 0.238f, 0.123f * H)) * Mat4::rotateZ(-s * 0.28f), Vec3(0, 0, 0), Vec3(0.068f, 0.016f, 0.02f), shade(pal.hair, 0.8f));
    }
    P.box(head, Vec3(0, 0.172f, 0.14f * H), Vec3(0.03f, 0.05f, 0.03f), skinDark);                 // nose
    P.box(head, Vec3(0, 0.128f, 0.127f * H), Vec3(0.056f, 0.01f, 0.016f), shade(pal.skin, 0.55f)); // mouth
    P.box(head, Vec3(0, 0.1f, 0.06f * H), Vec3(0.15f * H, 0.07f, 0.12f * H), pal.skin);             // jaw
}

void drawClothes(const Painter& P, const Mat4& root, const Mat4& spine, const Mat4& head, const Mat4 shoulder[2], const Mat4 elbow[2],
                 const std::array<RigLeg, 2>& legs, const Look& L, const Palette& pal, float t) {
    const float T = L.torso, A = L.arms;
    const float sway = std::sin(t * 2.4f), sway2 = std::sin(t * 3.1f + 1.0f);

    if (L.has(ACC_BELT)) {
        P.box(root, Vec3(0, 0.03f, 0), Vec3(0.365f * T, 0.065f, 0.245f * T), shade(pal.cloth, 0.55f));
        P.box(root, Vec3(0, 0.03f, 0.125f * T), Vec3(0.07f, 0.07f, 0.02f), pal.accent, 0.15f);
    }
    if (L.has(ACC_SASH)) {
        P.box(root, Vec3(0, 0.035f, 0), Vec3(0.36f * T, 0.085f, 0.24f * T), pal.cloth);
        P.ball(root, Vec3(0.1f, 0.035f, 0.125f * T), Vec3(0.07f, 0.07f, 0.05f), pal.cloth);
        for (int i = 0; i < 2; ++i) {
            Mat4 m = root * Mat4::translate(Vec3(0.1f + 0.045f * i, 0.0f, 0.125f * T)) * Mat4::rotateZ(0.12f * (i ? -1 : 1) + 0.08f * sway2 * (i ? -1 : 1)) * Mat4::rotateX(-0.1f);
            P.box(m, Vec3(0, -0.14f, 0), Vec3(0.05f, 0.28f - 0.04f * i, 0.012f), i ? shade(pal.cloth, 0.9f) : pal.cloth);
        }
    }
    if (L.has(ACC_COAT)) {
        // long tails behind and at the sides, high collar
        Mat4 backM = root * Mat4::translate(Vec3(0, -0.06f, -0.115f * T)) * Mat4::rotateX(0.10f + 0.07f * sway);
        P.box(backM, Vec3(0, -0.28f, 0), Vec3(0.38f * T, 0.56f, 0.04f), pal.cloth);
        P.box(backM, Vec3(0, -0.56f, 0), Vec3(0.38f * T, 0.025f, 0.045f), pal.accent, 0.1f);
        for (int s = -1; s <= 1; s += 2) {
            Mat4 sideM = root * Mat4::translate(Vec3(s * 0.185f * T, -0.05f, 0.0f)) * Mat4::rotateZ(-s * (0.08f + 0.05f * sway2));
            P.box(sideM, Vec3(0, -0.22f, 0), Vec3(0.04f, 0.46f, 0.2f * T), shade(pal.cloth, 0.92f));
        }
        P.box(spine, Vec3(0, 0.33f, 0), Vec3(0.43f * T, 0.35f, 0.27f * T), pal.cloth);          // coat front over the chest
        P.box(spine, Vec3(0, 0.33f, 0.138f * T), Vec3(0.05f, 0.35f, 0.012f), pal.accent, 0.1f);   // button line
        P.box(spine, Vec3(0, 0.56f, -0.055f), Vec3(0.30f, 0.16f, 0.1f), shade(pal.cloth, 0.8f));    // popped collar
        for (int s = -1; s <= 1; s += 2) P.box(spine * Mat4::rotateZ(-s * 0.3f), Vec3(s * 0.13f, 0.55f, 0.07f), Vec3(0.1f, 0.16f, 0.04f), shade(pal.cloth, 0.8f));
    }
    if (L.has(ACC_TABARD)) {
        // hanging cloth panels with gold hems; broad bands across the chest
        Mat4 f = root * Mat4::translate(Vec3(0, -0.04f, 0.125f * T)) * Mat4::rotateX(-0.04f * sway2);
        P.box(f, Vec3(0, -0.24f, 0), Vec3(0.20f, 0.48f, 0.025f), pal.cloth);
        P.box(f, Vec3(0, -0.47f, 0.004f), Vec3(0.2f, 0.035f, 0.03f), pal.accent, 0.1f);
        P.box(f, Vec3(0, -0.30f, 0.016f), Vec3(0.07f, 0.07f, 0.01f), pal.accent, 0.25f);
        Mat4 b = root * Mat4::translate(Vec3(0, -0.04f, -0.125f * T)) * Mat4::rotateX(0.05f * sway);
        P.box(b, Vec3(0, -0.24f, 0), Vec3(0.24f, 0.48f, 0.025f), shade(pal.cloth, 0.9f));
        P.box(b, Vec3(0, -0.47f, -0.004f), Vec3(0.24f, 0.035f, 0.03f), pal.accent, 0.1f);
        P.box(spine, Vec3(0, 0.46f, 0.0f), Vec3(0.2f, 0.6f * 0.1f, 0.3f * T), pal.cloth);         // stole collar
    }
    if (L.has(ACC_APRON)) {
        P.box(spine, Vec3(0, 0.2f, 0.128f * T), Vec3(0.33f * T, 0.5f, 0.03f), pal.cloth);
        P.box(spine, Vec3(0, 0.12f, 0.15f * T), Vec3(0.2f * T, 0.1f, 0.025f), shade(pal.cloth, 0.7f));   // pocket
        P.box(root, Vec3(0, -0.2f, 0.115f * T), Vec3(0.30f * T, 0.34f, 0.03f), shade(pal.cloth, 0.95f));
        for (int s = -1; s <= 1; s += 2) P.box(spine * Mat4::rotateZ(s * 0.1f), Vec3(s * 0.145f * T, 0.43f, 0.0f), Vec3(0.045f, 0.12f, 0.27f * T), shade(pal.cloth, 0.7f));
    }
    if (L.has(ACC_SKIRT)) {
        // layered dancer skirt: petals fanned around the hips
        for (int k = 0; k < 10; ++k) {
            float a = (float)k / 10.0f * 2.0f * PI;
            float lift = 0.06f * std::sin(t * 2.6f + k * 1.3f);
            Mat4 m = root * Mat4::rotateY(a) * Mat4::translate(Vec3(0, -0.05f, 0.085f)) * Mat4::rotateX(0.72f + lift);
            P.box(m, Vec3(0, 0, 0.1f), Vec3(0.135f, 0.022f, 0.22f), (k & 1) ? pal.cloth : pal.body);
        }
    }
    if (L.has(ACC_SCARF)) {
        P.tube(spine, Vec3(0, 0.52f, 0), Vec3(0.2f, 0.07f, 0.2f), pal.accent);
        Mat4 m = spine * Mat4::translate(Vec3(0.05f, 0.5f, -0.12f * T)) * Mat4::rotateX(0.35f + 0.12f * sway);
        P.box(m, Vec3(0, -0.22f, 0), Vec3(0.09f, 0.46f, 0.02f), pal.accent);
        P.box(m * Mat4::translate(Vec3(0, -0.46f, 0)) * Mat4::rotateX(0.2f * sway2), Vec3(0, -0.07f, 0), Vec3(0.09f, 0.15f, 0.018f), shade(pal.accent, 0.85f));
    }
    if (L.has(ACC_VISOR)) {
        P.box(head, Vec3(0, 0.205f, 0.125f * L.head), Vec3(0.235f, 0.03f, 0.03f), Color(0.04f, 0.03f, 0.08f));
        for (int s = -1; s <= 1; s += 2) P.box(head, Vec3(s * 0.058f, 0.205f, 0.138f * L.head), Vec3(0.09f, 0.055f, 0.016f), mix(pal.accent, Color(0.7f, 0.5f, 1.0f), 0.5f), 0.9f);
    }
    if (L.has(ACC_GOGGLES)) {
        P.tube(head, Vec3(0, 0.265f, 0), Vec3(0.30f, 0.045f, 0.30f), Color(0.18f, 0.12f, 0.08f));
        for (int s = -1; s <= 1; s += 2) {
            Mat4 m = head * Mat4::translate(Vec3(s * 0.058f, 0.285f, 0.125f)) * Mat4::rotateX(PI * 0.5f - 0.35f);
            P.tube(m, Vec3(0, 0, 0), Vec3(0.105f, 0.05f, 0.105f), Color(0.35f, 0.22f, 0.1f));
            P.tube(m, Vec3(0, 0.02f, 0), Vec3(0.08f, 0.04f, 0.08f), Color(1.0f, 0.65f, 0.2f), 0.9f);
        }
    }
    if (L.has(ACC_HEADBAND)) {
        P.tube(head, Vec3(0, 0.255f, -0.002f), Vec3(0.295f, 0.04f, 0.295f), pal.accent);
        for (int i = 0; i < 2; ++i) {
            Mat4 m = head * Mat4::translate(Vec3(0.02f - 0.04f * i, 0.255f, -0.14f)) * Mat4::rotateX(0.55f + 0.2f * i + 0.18f * sway) * Mat4::rotateZ((i ? -1 : 1) * 0.12f);
            P.box(m, Vec3(0, -0.15f, 0), Vec3(0.055f, 0.34f - 0.06f * i, 0.012f), i ? shade(pal.accent, 0.85f) : pal.accent);
        }
    }
    if (L.has(ACC_BEARD)) {
        P.box(head, Vec3(0, 0.105f, 0.085f * L.head), Vec3(0.16f * L.head, 0.1f, 0.1f), pal.hair);
        P.box(head, Vec3(0, 0.055f, 0.09f * L.head), Vec3(0.12f * L.head, 0.09f, 0.085f), shade(pal.hair, 0.9f));
        P.box(head, Vec3(0, 0.153f, 0.13f * L.head), Vec3(0.1f, 0.022f, 0.035f), pal.hair);
    }
    if (L.has(ACC_HAMMER)) {
        // a great forge hammer carried across the back
        Mat4 m = spine * Mat4::translate(Vec3(-0.06f, 0.26f, -0.2f * T)) * Mat4::rotateZ(0.65f);
        P.box(m, Vec3(0, 0.05f, 0), Vec3(0.05f, 0.95f, 0.05f), Color(0.42f, 0.26f, 0.12f));
        P.box(m, Vec3(0, 0.5f, 0), Vec3(0.30f, 0.2f, 0.2f), Color(0.38f, 0.38f, 0.42f));
        P.box(m, Vec3(0, 0.5f, 0.0f), Vec3(0.32f, 0.05f, 0.22f), Color(1.0f, 0.5f, 0.12f), 0.8f);
        P.box(m, Vec3(0, -0.40f, 0), Vec3(0.065f, 0.1f, 0.065f), Color(0.2f, 0.2f, 0.22f));
    }

    for (int i = 0; i < 2; ++i) {
        const float sx = i == 0 ? 1.0f : -1.0f;
        if (L.has(ACC_PAULDRONS)) {
            P.ball(shoulder[i], Vec3(sx * 0.02f, 0.035f, 0), Vec3(0.27f, 0.17f, 0.25f) * (0.85f + 0.15f * A), shade(pal.body, 1.15f));
            P.tube(shoulder[i], Vec3(sx * 0.02f, -0.03f, 0), Vec3(0.255f, 0.025f, 0.235f) * (0.85f + 0.15f * A), pal.accent, 0.12f);
            P.spike(shoulder[i], Vec3(sx * 0.07f, 0.1f, 0), Vec3(sx * 0.7f, 1.0f, 0), 0.11f, 0.07f, pal.accent, 0.1f);
        }
        if (L.has(ACC_WRAPS)) {
            for (int k = 0; k < 3; ++k)
                P.box(elbow[i], Vec3(0, -0.07f - 0.075f * k, 0), Vec3(0.125f * A, 0.045f, 0.125f * A), pal.accent);
            P.box(elbow[i], Vec3(0, -0.315f, 0.01f), Vec3(0.14f * A, 0.07f, 0.14f * A), pal.accent);
        }
        if (L.has(ACC_GAUNTLETS)) {
            P.box(elbow[i], Vec3(0, -0.15f, 0), Vec3(0.165f * A, 0.2f, 0.165f * A), shade(pal.body, 0.9f));
            P.box(elbow[i], Vec3(0, -0.05f, 0), Vec3(0.18f * A, 0.035f, 0.18f * A), pal.accent, 0.15f);
            P.box(elbow[i], Vec3(0, -0.315f, 0.01f), Vec3(0.15f * A, 0.115f, 0.15f * A), shade(pal.body, 0.8f));
        }
        if (L.has(ACC_RIBBONS)) {
            Mat4 m = elbow[i] * Mat4::translate(Vec3(0, -0.27f, 0)) * Mat4::rotateX(0.5f + 0.25f * std::sin(t * 3.2f + i * 2.0f)) * Mat4::rotateZ(sx * 0.1f);
            P.box(m, Vec3(0, -0.2f, 0), Vec3(0.06f, 0.42f, 0.012f), pal.accent, 0.12f);
            P.box(m * Mat4::translate(Vec3(0, -0.4f, 0)) * Mat4::rotateX(0.3f * std::sin(t * 4.0f + i)), Vec3(0, -0.1f, 0), Vec3(0.05f, 0.22f, 0.012f), pal.body, 0.12f);
        }
        if (L.has(ACC_BOOTS)) {
            const RigLeg& lg = legs[i];
            P.put(P.boxMesh, lg.shin * Mat4::translate(Vec3(0, -0.2f, 0)) * Mat4::scale(Vec3(1.28f, 0.52f, 1.22f)), pal.boots, 0);
            P.put(P.boxMesh, lg.shin * Mat4::translate(Vec3(0, 0.07f, 0)) * Mat4::scale(Vec3(1.36f, 0.08f, 1.3f)), shade(pal.boots, 1.5f), 0);
        }
    }
}

} // namespace

bool RigRenderer::init() {
    MeshBuilder b;
    b.addBox(Vec3(0, 0, 0), Vec3(1, 1, 1), Vec3(1, 1, 1));
    box_.upload(b);
    b.clear();
    b.addSphere(Vec3(0, 0, 0), 0.5f, 12, 18, Vec3(1, 1, 1));
    sphere_.upload(b);
    b.clear();
    b.addCylinder(Vec3(0, -0.5f, 0), 0.5f, 1.0f, 16, Vec3(1, 1, 1));
    cyl_.upload(b);
    // unit cone: base circle at y = -0.5 (radius 0.5), apex at y = +0.5, smooth side normals
    b.clear();
    const int seg = 10;
    const Vec3 white(1, 1, 1);
    for (int i = 0; i < seg; ++i) {
        float a0 = (float)i / seg * 2 * PI, a1 = (float)(i + 1) / seg * 2 * PI, am = (a0 + a1) * 0.5f;
        Vec3 p0(std::cos(a0) * 0.5f, -0.5f, std::sin(a0) * 0.5f), p1(std::cos(a1) * 0.5f, -0.5f, std::sin(a1) * 0.5f), apex(0, 0.5f, 0);
        Vec3 n0 = normalize(Vec3(std::cos(a0), 0.5f, std::sin(a0))), n1 = normalize(Vec3(std::cos(a1), 0.5f, std::sin(a1)));
        Vec3 nm = normalize(Vec3(std::cos(am), 0.5f, std::sin(am)));
        b.addTriangle({p0, n0, white}, {apex, nm, white}, {p1, n1, white});
        b.addTriangle({Vec3(0, -0.5f, 0), Vec3(0, -1, 0), white}, {p0, Vec3(0, -1, 0), white}, {p1, Vec3(0, -1, 0), white});
    }
    cone_.upload(b);
    return true;
}

void RigRenderer::draw(Renderer& r, const Pose& p, const Vec3& feet, float yaw, const Character& c, float scale,
                       float alpha, float flash, bool asStand, bool castShadow, float floorY) {
    const Look& L = c.look;
    const Color styleBase = c.styleColor;
    Palette pal;
    pal.body = asStand ? styleBase : c.bodyColor;
    pal.accent = asStand ? mix(styleBase, Color(1, 1, 1), 0.5f) : c.accentColor;
    pal.skin = asStand ? mix(styleBase, Color(1, 1, 1), 0.3f) : L.skin;
    pal.hair = asStand ? mix(styleBase, Color(1, 1, 1), 0.7f) : c.hairColor;
    pal.dark = asStand ? styleBase * 0.8f : c.bodyColor * 0.6f;
    pal.pants = asStand ? styleBase * 0.8f : L.pants;
    pal.boots = asStand ? mix(styleBase, Color(1, 1, 1), 0.5f) : L.boots;
    pal.cloth = asStand ? styleBase * 0.9f : L.cloth;
    pal.eye = Color(0.05f, 0.04f, 0.07f);
    float em = asStand ? 0.55f : 0.0f;
    if (flash > 0) {
        for (Color* col : {&pal.body, &pal.accent, &pal.skin, &pal.hair, &pal.dark, &pal.pants, &pal.boots, &pal.cloth})
            *col = mix(*col, Color(1, 1, 1), flash);
        em = std::max(em, flash * 0.8f);
    }
    Painter P{r, &box_, &sphere_, &cyl_, &cone_, alpha, em, castShadow};

    const float heightScale = (c.height / 1.85f) * scale;
    const Mat4 root = rigRoot(p, feet, yaw, heightScale, asStand, floorY);
    const auto legs = rigLegs(p, root);
    const float S = L.shoulders, T = L.torso, A = L.arms, G = L.legs;
    const bool vest = L.has(ACC_VEST) && !asStand, bareArms = L.has(ACC_BARE_ARMS) && !asStand;

    // ---- core: pelvis, waist, chest, shoulder yoke
    P.box(root, Vec3(0, 0.0f, 0), Vec3(0.35f * T, 0.2f, 0.225f * T), pal.dark);
    Mat4 spine = root * Mat4::translate(Vec3(0, 0.1f, 0)) * Mat4::rotateX(p.spinePitch) * Mat4::rotateY(p.spineYaw) * Mat4::rotateZ(p.spineRoll);
    P.box(spine, Vec3(0, 0.09f, 0), Vec3(0.31f * T, 0.23f, 0.215f * T), vest ? pal.skin : pal.body);
    P.box(spine, Vec3(0, 0.33f, 0), Vec3(0.40f * T, 0.34f, 0.25f * T), vest ? pal.skin : pal.body);
    if (vest) {
        // open vest: cloth panels either side of a bare chest, with a pectoral line
        for (int s = -1; s <= 1; s += 2) {
            P.box(spine, Vec3(s * 0.15f * T, 0.31f, 0.006f), Vec3(0.115f * T, 0.38f, 0.262f * T), pal.body);
            P.box(spine, Vec3(s * 0.15f * T, 0.31f, 0.132f * T), Vec3(0.012f, 0.38f, 0.01f), pal.accent, 0.1f);
            P.box(spine, Vec3(s * 0.055f * T, 0.30f, 0.127f * T), Vec3(0.09f * T, 0.1f, 0.01f), shade(pal.skin, 0.88f));   // pec
        }
        P.box(spine, Vec3(0, 0.09f, 0.108f * T), Vec3(0.02f, 0.2f, 0.01f), shade(pal.skin, 0.8f));                    // abs line
        P.box(spine, Vec3(0, 0.33f, -0.1f * T), Vec3(0.40f * T, 0.34f, 0.06f), pal.body);                                // vest back
    } else {
        P.box(spine, Vec3(0, 0.455f, 0.005f), Vec3(0.455f * S * (0.5f + 0.5f * T), 0.115f, 0.265f * T), pal.accent);
    }

    // ---- head
    Mat4 neck = spine * Mat4::translate(Vec3(0, 0.5f, 0)) * Mat4::rotateX(p.headPitch) * Mat4::rotateY(p.headYaw);
    P.box(neck, Vec3(0, 0.03f, 0), Vec3(0.105f * T, 0.09f, 0.105f * T), pal.skin);
    P.ball(neck, Vec3(0, 0.19f, 0), Vec3(0.27f, 0.285f, 0.27f) * L.head, pal.skin);
    if (!asStand) drawFace(P, neck, L, pal);
    else P.box(neck, Vec3(0, 0.1f, 0.06f * L.head), Vec3(0.15f * L.head, 0.07f, 0.12f * L.head), pal.skin);
    if (!asStand) drawHair(P, neck, L, pal, time_);
    else if (L.hair != HairStyle::Bald) P.ball(neck, Vec3(0, 0.235f, -0.04f) * L.head, Vec3(0.295f, 0.25f, 0.31f) * L.head, pal.hair);

    // ---- arms and legs
    Mat4 shoulder[2], elbow[2];
    for (int i = 0; i < 2; ++i) {
        const float sx = i == 0 ? 1.0f : -1.0f;   // local +x is the character's left when facing +z
        shoulder[i] = spine * Mat4::translate(Vec3(sx * 0.25f * S * (0.55f + 0.45f * T), 0.42f, 0)) * Mat4::rotateZ(sx * p.armOut[i]) * Mat4::rotateX(-p.armPitch[i]);
        elbow[i] = shoulder[i] * Mat4::translate(Vec3(0, -0.3f, 0)) * Mat4::rotateX(-p.elbow[i]);
        const Color upper = bareArms ? pal.skin : pal.body;
        const Color fore = bareArms ? pal.skin : pal.accent;
        P.ball(shoulder[i], Vec3(0, 0, 0), Vec3(0.17f, 0.17f, 0.17f) * A, upper);
        P.box(shoulder[i], Vec3(0, -0.15f, 0), Vec3(0.13f * A, 0.3f, 0.13f * A), upper);
        P.ball(elbow[i], Vec3(0, 0, 0), Vec3(0.12f, 0.12f, 0.12f) * A, upper);
        P.box(elbow[i], Vec3(0, -0.14f, 0), Vec3(0.11f * A, 0.28f, 0.11f * A), fore);
        P.box(elbow[i], Vec3(0, -0.325f, 0.012f), Vec3(0.125f * A, 0.115f, 0.13f * A), pal.skin);   // fist
        P.box(elbow[i], Vec3(0, -0.37f, 0.045f), Vec3(0.105f * A, 0.03f, 0.04f), shade(pal.skin, 0.85f));   // knuckles
        // legs: same boxes as the grounding code uses, thickened sideways only
        const float lg = (0.5f + 0.5f * G);
        P.put(P.boxMesh, legs[i].thigh * Mat4::scale(Vec3(lg, 1, lg)), pal.pants, 0);
        P.put(P.boxMesh, legs[i].shin * Mat4::scale(Vec3(lg, 1, lg)), shade(pal.pants, 1.12f), 0);
        P.put(P.boxMesh, legs[i].foot, L.has(ACC_BOOTS) || asStand ? pal.boots : pal.skin, 0);
    }

    if (asStand) drawStandOrnaments(P, spine, neck, shoulder, L, styleBase, time_);
    else drawClothes(P, root, spine, neck, shoulder, elbow, legs, L, pal, time_);
}

} // namespace dc
