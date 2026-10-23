// Hunt phase: the player stalks corridors looking for opponents. Finding one starts a fight.
#pragma once
#include <vector>
#include "core/math.h"
#include "gfx/renderer.h"
#include "game/maze.h"
#include "game/rig.h"
#include "game/input.h"

namespace dc {

struct HuntOpponent {
    int charId = 0;
    Vec3 pos;
    float yaw = 0;
    int targetCell = -1;
    float idleTimer = 0;
    float alert = 0;         // 0 = unaware, 1 = hunting the player
    float losLost = 0;
    bool defeated = false;
    bool moving = false;
    float walkPhase = 0;
    float speedMul = 1.0f;
};

class Hunt {
public:
    void init(int playerChar, const std::vector<int>& opponentChars, uint64_t seed);
    // Returns the index of an opponent the player engaged this frame, or -1.
    int update(float dt, const PadState& pad, bool engageAllowed);
    void render(Renderer& r, RigRenderer& rig, float time);
    void markDefeated(int idx);
    void resumeAfterFight();

    int remaining() const;
    const std::vector<HuntOpponent>& opponents() const { return opps_; }
    const Maze& maze() const { return maze_; }
    Vec3 playerPos() const { return playerPos_; }
    float senseLevel() const { return senseLevel_; }        // 0..1 closeness of the nearest opponent
    float heartPulse() const { return heartPulse_; }        // 0..1 pulse animation
    int senseDirection() const { return senseDir_; }        // -1 none, else 0..3 = N,E,S,W (next corridor step)
    float cameraYaw() const { return camYaw_; }
    int engageCandidate() const { return engageCandidate_; }
    int nearestVisible() const { return nearestVisible_; }
    const Camera& camera() const { return cam_; }
    bool anyOpponentHunting() const;
    float worldTime() const { return time_; }

private:
    void updateOpponent(HuntOpponent& o, float dt);
    void updateSense();
    void updateCamera(float dt, const PadState& pad);
    float yawToward(const Vec3& dir) const { return std::atan2(dir.x, dir.z); }

    Maze maze_;
    Mesh floor_, walls_, ceiling_, props_;
    int playerChar_ = 0;
    Vec3 playerPos_;
    float playerYaw_ = 0;
    float camYaw_ = 0, camPitch_ = 0.32f;
    float walkPhase_ = 0;
    float moveBlend_ = 0;
    bool running_ = false;
    Camera cam_;
    std::vector<HuntOpponent> opps_;
    Rng rng_{1};
    float senseLevel_ = 0, heartPulse_ = 0, heartTimer_ = 0;
    int senseDir_ = -1;
    int engageCandidate_ = -1;
    int nearestVisible_ = -1;
    float time_ = 0;
    float senseRefresh_ = 0;
};

} // namespace dc
