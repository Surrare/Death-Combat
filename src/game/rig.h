// Procedural humanoid rig: poses are generated from fighter state in code, no animation assets.
#pragma once
#include "core/math.h"
#include "gfx/renderer.h"
#include "game/fight.h"

namespace dc {

struct Pose {
    float hipHeight = 0.9f;
    Vec3 rootOffset;             // extra translation (local: x = right/left, z = forward)
    float rootPitch = 0, rootRoll = 0, bodyYaw = 0;
    float spinePitch = 0, spineYaw = 0, spineRoll = 0;
    float headPitch = 0, headYaw = 0;
    float armPitch[2] = {0.9f, 0.9f};   // 0 = left, 1 = right; positive = forward
    float armOut[2] = {0.25f, 0.25f};   // positive = away from body
    float elbow[2] = {1.8f, 1.8f};      // bend (0 = straight)
    float legPitch[2] = {0, 0};
    float legOut[2] = {0.15f, 0.15f};
    float knee[2] = {0.2f, 0.2f};
};

Pose lerpPose(const Pose& a, const Pose& b, float t);
Pose idlePose(float time);
Pose standIdlePose(float time);      // floating guardian pose for Stands
Pose poseForFighter(const Fighter& f, float time);
Pose attackPose(AnimKind kind, float phase, float time, bool variant);   // phase: 0..1 startup, 1..2 active, 2..3 recovery

class RigRenderer {
public:
    bool init();
    // feet position, yaw (radians, 0 = facing +z), colours. alpha < 1 renders the ghostly Stand.
    void draw(Renderer& r, const Pose& p, const Vec3& feet, float yaw, const Character& c, float scale,
              float alpha = 1.0f, float flash = 0.0f, bool asStand = false, bool castShadow = true);
    const Mesh& unitBox() const { return box_; }
    const Mesh& unitSphere() const { return sphere_; }

private:
    Mesh box_, sphere_;
};

} // namespace dc
