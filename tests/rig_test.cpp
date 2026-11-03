// Geometry regression: the actual rendered leg boxes must stay above the floor.
// No window or OpenGL calls are needed; inspect all eight corners of each box.
#include "game/rig.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace dc;

namespace {
constexpr float TOLERANCE = 0.00002f;
int checks = 0, failures = 0;

void check(bool passed, const char* description) {
    ++checks;
    if (!passed) {
        if (failures < 12) std::printf("FAIL: %s\n", description);
        ++failures;
    }
}

float lowestLegVertex(const Pose& pose, const Mat4& root) {
    float lowest = std::numeric_limits<float>::max();
    for (const RigLeg& leg : rigLegs(pose, root)) {
        for (const Mat4& box : {leg.thigh, leg.shin, leg.foot}) {
            for (float x : {-0.5f, 0.5f})
                for (float y : {-0.5f, 0.5f})
                    for (float z : {-0.5f, 0.5f})
                        lowest = std::min(lowest, box.transformPoint(Vec3(x, y, z)).y);
        }
    }
    return lowest;
}

void checkPose(const Pose& pose, const Vec3& feet, float yaw, float scale, float floor) {
    const Mat4 root = rigRoot(pose, feet, yaw, scale, false, floor);
    const Mat4 authored = rigRoot(pose, feet, yaw, scale, true, floor);
    const float lowest = lowestLegVertex(pose, root);
    const float before = lowestLegVertex(pose, authored);
    check(std::isfinite(lowest) && lowest >= floor - TOLERANCE, "leg penetrates surface");
    if (before < floor)
        check(std::fabs(lowest - floor) < TOLERANCE, "penetrating pose must rest on surface");
    else
        check(std::fabs(root.m[3][1] - authored.m[3][1]) < TOLERANCE, "clear pose must keep its height");
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (c != 3 || r != 1)
                check(root.m[c][r] == authored.m[c][r], "grounding must only translate vertically");
}
}

int main() {
    // Regression against the original idle pose: its feet were about 0.21 below zero.
    const Pose idle = idlePose(0);
    const float oldBottom = lowestLegVertex(idle, rigRoot(idle, Vec3(), 0, 1, true));
    check(oldBottom < -0.20f, "regression fixture must reproduce original penetration");

    for (const Character& character : roster()) {
        Fighter fighter;
        fighter.ch = &character;
        fighter.stateLen = 40;
        for (float scale : {character.height / 1.85f, character.height / 1.85f * 1.35f}) {
            for (float floor : {0.0f, 0.01f}) {
                // Includes standing, crouching, walking, jumps, knockdowns and getting up.
                for (int state = static_cast<int>(FState::Intro); state <= static_cast<int>(FState::Taunt); ++state) {
                    fighter.state = static_cast<FState>(state);
                    for (int frame = 0; frame <= 40; ++frame) {
                        fighter.timer = frame;
                        fighter.vy = 0.2f - frame * 0.01f;
                        fighter.pos.y = state == static_cast<int>(FState::Jump) ? 1.0f : 0.0f;
                        const Pose pose = poseForFighter(fighter, frame / 15.0f);
                        checkPose(pose, fighter.pos, 1.2f, scale, floor);
                    }
                }
                // Full windup/strike/recovery for every move, including crouching and air attacks.
                fighter.state = FState::Attack;
                for (const Move& move : character.moves) {
                    fighter.move = &move;
                    for (int frame = 0; frame <= move.totalFrames(); ++frame) {
                        fighter.moveFrame = frame;
                        fighter.pos.y = move.stance == Stance::Air ? 0.7f : 0.0f;
                        checkPose(poseForFighter(fighter, frame / 60.0f), fighter.pos, -1.2f, scale, floor);
                    }
                }
                fighter.move = nullptr;
            }
        }
    }

    // Hunt uses its own blend of walk/idle angles; also exercise root roll and offset.
    for (int frame = 0; frame <= 180; ++frame) {
        const float time = frame / 30.0f;
        const float swing = std::sin(time * 9.0f);
        Pose pose = idlePose(time);
        pose.legPitch[0] = 0.55f * swing; pose.legPitch[1] = -0.55f * swing;
        pose.knee[0] = 0.4f + 0.5f * std::max(0.0f, -swing);
        pose.knee[1] = 0.4f + 0.5f * std::max(0.0f, swing);
        checkPose(pose, Vec3(3, 0, -2), time, 1.0f, 0.0f);
        pose.rootRoll = 0.2f; pose.rootOffset.y = -0.1f;
        checkPose(pose, Vec3(3, 0, -2), time, 1.2f, 0.0f);
    }

    // An airborne model keeps its authored height; Stands remain free to float.
    checkPose(idle, Vec3(0, 2, 0), 0, 1, 0.01f);
    check(std::fabs(lowestLegVertex(idle, rigRoot(idle, Vec3(0, 2, 0), 0, 1)) - (oldBottom + 2)) < TOLERANCE,
          "jump altitude must not be snapped to the floor");
    const Mat4 stand = rigRoot(standIdlePose(1), Vec3(0, 0.3f, 0), 0.7f, 1.08f, true, 0);
    const Mat4 standRaisedFloor = rigRoot(standIdlePose(1), Vec3(0, 0.3f, 0), 0.7f, 1.08f, true, 5);
    check(stand.m[3][1] == standRaisedFloor.m[3][1], "Stand must ignore ground correction");
    std::printf("Rig grounding: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
