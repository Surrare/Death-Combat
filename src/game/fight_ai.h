// CPU opponent for the fight. Produces SimInput like a player would.
#pragma once
#include "game/fight.h"

namespace dc {

class FightAI {
public:
    void reset(int difficulty, uint64_t seed);
    SimInput think(const FightSim& sim, int me);

private:
    const Move* findNormal(const Character& c, int level, Stance stance, bool standOn) const;
    const Move* findSpecial(const Character& c, Motion motion, int strength, bool standOn) const;
    const Move* findKind(const Character& c, MoveKind kind) const;
    const Move* pickAttack(const Fighter& me, const Fighter& opp, float dist);

    int difficulty_ = 1;
    Rng rng_{777};
    int thinkTimer_ = 0;
    int holdDir_ = 0, holdDirY_ = 0, holdFrames_ = 0;
    int blockFrames_ = 0;
    bool blockLow_ = false;
    int reactionDelay_ = 0;
    int comboStep_ = 0;
    const Move* planned_ = nullptr;
    int mashTimer_ = 0;
};

} // namespace dc
