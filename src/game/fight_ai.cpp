#include "game/fight_ai.h"
#include <cmath>

namespace dc {

void FightAI::reset(int difficulty, uint64_t seed) {
    difficulty_ = difficulty;
    rng_ = Rng(seed);
    thinkTimer_ = 0; holdDir_ = 0; holdDirY_ = 0; holdFrames_ = 0; blockFrames_ = 0; comboStep_ = 0; planned_ = nullptr;
}

const Move* FightAI::findNormal(const Character& c, int level, Stance stance, bool standOn) const {
    for (const Move& m : c.moves) {
        if (m.kind != MoveKind::Normal || m.chainLevel != level || m.stance != stance) continue;
        if (c.style == StyleType::Stand && m.stand != StandUse::Any && ((m.stand == StandUse::On) != standOn)) continue;
        return &m;
    }
    return nullptr;
}

const Move* FightAI::findSpecial(const Character& c, Motion motion, int strength, bool standOn) const {
    const uint32_t btn[3] = {BTN_LIGHT, BTN_MEDIUM, BTN_HEAVY};
    for (const Move& m : c.moves) {
        if (m.kind != MoveKind::Special || m.motion != motion || m.buttons != btn[strength]) continue;
        if (c.style == StyleType::Stand && m.stand != StandUse::Any && ((m.stand == StandUse::On) != standOn)) continue;
        return &m;
    }
    return nullptr;
}

const Move* FightAI::findKind(const Character& c, MoveKind kind) const {
    for (const Move& m : c.moves) if (m.kind == kind) return &m;
    return nullptr;
}

const Move* FightAI::pickAttack(const Fighter& me, const Fighter& opp, float dist) {
    const Character& c = *me.ch;
    float r = rng_.uniform();
    bool close = dist < 1.6f;
    bool mid = dist < 2.6f;
    if (close && r < 0.12f) return findKind(c, MoveKind::Throw);
    if (close && r < 0.2f) {
        const Move* ct = findSpecial(c, Motion::QCB, rng_.range(0, 2), me.styleOn);
        if (ct && ct->commandThrow) return ct;
    }
    if (mid && r < 0.55f) {
        int lvl = rng_.range(0, 2);
        Stance st = rng_.chance(0.3f) ? Stance::Crouch : Stance::Ground;
        return findNormal(c, lvl, st, me.styleOn);
    }
    if (r < 0.75f) {
        Motion mo = rng_.chance(0.6f) ? Motion::QCF : Motion::QCB;
        const Move* s = findSpecial(c, mo, rng_.range(0, 2), me.styleOn);
        if (s && s->commandThrow && !close) s = findSpecial(c, Motion::QCF, 0, me.styleOn);
        return s;
    }
    if (mid) return findSpecial(c, Motion::DP, rng_.range(0, 2), me.styleOn);
    return findNormal(c, 1, Stance::Ground, me.styleOn);
}

SimInput FightAI::think(const FightSim& sim, int meIdx) {
    SimInput in;
    const Fighter& me = sim.fighter(meIdx);
    const Fighter& opp = sim.fighter(1 - meIdx);
    const Character& c = *me.ch;
    if (sim.phase() != Phase::Fight) return in;

    // Rush Mode clash: mash
    if (sim.clash().active) {
        float rate = difficulty_ == 0 ? 0.22f : (difficulty_ == 1 ? 0.33f : 0.45f);
        if (rng_.chance(rate)) in.pressed = BTN_LIGHT;
        return in;
    }

    float dx = opp.pos.x - me.pos.x;
    float dist = std::fabs(dx);
    int toward = dx > 0 ? 1 : -1;
    auto relToAbs = [&](int rel) { return rel * toward; };

    float reactChance = difficulty_ == 0 ? 0.25f : (difficulty_ == 1 ? 0.5f : 0.8f);

    // ---- defend: opponent attacking in range
    bool oppThreat = opp.attacking() && opp.move && opp.move->kind != MoveKind::Taunt && dist < 3.0f && opp.moveFrame < opp.move->startup + opp.move->active;
    bool oppProjectileThreat = false;
    for (const Projectile& p : sim.projectiles())
        if (p.owner != meIdx && (p.pos.x - me.pos.x) * p.vx < 0 && std::fabs(p.pos.x - me.pos.x) < 3.0f) oppProjectileThreat = true;

    if ((oppThreat || oppProjectileThreat) && blockFrames_ <= 0 && me.canAct()) {
        if (rng_.chance(reactChance)) {
            bool low = false;
            if (opp.move) for (const HitWindow& w : opp.move->hits) if (w.low) low = true;
            if (opp.move && opp.move->commandThrow) {
                // escape the throw: backdash or jump
                if (rng_.chance(0.5f)) { in.pressed |= BTN_DASH; in.dirX = relToAbs(-1); }
                else in.dirY = 1;
                return in;
            }
            if (opp.move && opp.move->rush && rng_.chance(0.3f)) {
                // sidestep the rush (stylish evade)
                in.pressed |= BTN_DASH; in.dirY = rng_.chance(0.5f) ? 1 : -1;
                return in;
            }
            if (opp.move && (opp.move->kind == MoveKind::HHA || opp.move->kind == MoveKind::GHA) && rng_.chance(0.4f)) {
                in.pressed |= BTN_DASH; in.dirY = 1;
                return in;
            }
            blockFrames_ = 14 + rng_.range(0, 14);
            blockLow_ = low || rng_.chance(0.35f);
        }
    }
    if (blockFrames_ > 0) {
        blockFrames_--;
        in.dirX = relToAbs(-1);
        in.dirY = blockLow_ ? -1 : 0;
        // Stylish evade out of blockstun sometimes
        if ((me.state == FState::Block || me.state == FState::CrouchBlock) && rng_.chance(0.03f * difficulty_)) { in.pressed |= BTN_DASH; in.dirY = 1; }
        return in;
    }

    // ---- continue combos
    if (me.attacking() && me.move && me.moveConnected) {
        if (me.move->kind == MoveKind::Normal) {
            int lvl = me.move->chainLevel;
            if (lvl < 2 && rng_.chance(0.6f + 0.15f * difficulty_)) {
                in.requestMove = findNormal(c, lvl + 1, me.move->stance == Stance::Air ? Stance::Air : Stance::Ground, me.styleOn);
                return in;
            }
            if (me.move->stance != Stance::Air) {
                if (me.hhg >= 2000 && rng_.chance(0.5f)) { in.requestMove = findKind(c, MoveKind::GHA); return in; }
                if (me.hhg >= 1000 && rng_.chance(0.5f)) { in.requestMove = findKind(c, MoveKind::HHA); return in; }
                Motion mo = rng_.chance(0.5f) ? Motion::QCF : Motion::DP;
                const Move* s = findSpecial(c, mo, rng_.range(0, 2), me.styleOn);
                if (s && !s->commandThrow && rng_.chance(0.7f)) { in.requestMove = s; return in; }
            }
        } else if (me.move->kind == MoveKind::Special) {
            if (me.hhg >= 2000 && rng_.chance(0.35f)) { in.requestMove = findKind(c, MoveKind::GHA); return in; }
            if (me.hhg >= 1000 && rng_.chance(0.45f)) { in.requestMove = findKind(c, MoveKind::HHA); return in; }
        }
        return in;
    }

    // ---- punish: opponent is recovering / stunned
    bool oppOpen = (opp.state == FState::StyleBreak) || (opp.attacking() && opp.move && opp.moveFrame > opp.move->startup + opp.move->active && (opp.move->totalFrames() - opp.moveFrame) > 10);
    if (oppOpen && dist < 2.4f && me.canAct() && rng_.chance(reactChance)) {
        if (opp.state == FState::StyleBreak && me.hhg >= 2000) { in.requestMove = findKind(c, MoveKind::GHA); return in; }
        in.requestMove = findNormal(c, rng_.range(0, 1), Stance::Ground, me.styleOn);
        return in;
    }

    // ---- anti-air
    if (!opp.grounded() && opp.vy < 0.05f && dist < 2.2f && me.canAct() && rng_.chance(reactChance * 0.8f)) {
        in.requestMove = findSpecial(c, Motion::DP, 1, me.styleOn);
        if (!in.requestMove) in.requestMove = findNormal(c, 2, Stance::Ground, me.styleOn);
        return in;
    }

    // ---- air attack while jumping toward the opponent
    if (me.state == FState::Jump && !me.airAttackUsed && dist < 1.8f && me.vy < 0.08f) {
        in.requestMove = findNormal(c, rng_.range(1, 2), Stance::Air, me.styleOn);
        return in;
    }

    // ---- opponent knocked down: wait, taunt or position
    if (opp.isDown()) {
        if (me.canAct() && rng_.chance(0.01f) && dist > 1.5f) { in.pressed |= BTN_TAUNT; return in; }
        if (dist > 2.0f) in.dirX = relToAbs(1);
        return in;
    }

    // ---- style management
    if (me.canAct() && me.styleLock == 0) {
        if (c.style == StyleType::Stand) {
            if (!me.styleOn && me.styleGauge > 0.6f && dist < 3.5f && rng_.chance(0.02f)) { in.pressed |= BTN_STYLE; return in; }
            if (me.styleOn && me.styleGauge < 0.25f && rng_.chance(0.08f)) { in.pressed |= BTN_STYLE; return in; }
        } else {
            if (!me.styleOn && me.styleGauge > 0.7f && rng_.chance(0.01f)) { in.pressed |= BTN_STYLE; return in; }
        }
    }

    // ---- neutral decisions, re-evaluated every few frames
    if (holdFrames_ > 0) {
        holdFrames_--;
        in.dirX = holdDir_ != 0 ? relToAbs(holdDir_) : 0;
        in.dirY = holdDirY_;
        if (planned_ && me.canAct()) { in.requestMove = planned_; planned_ = nullptr; holdFrames_ = 0; }
        return in;
    }
    if (!me.canAct()) return in;

    float r = rng_.uniform();
    bool hasProjectile = findSpecial(c, Motion::QCF, 0, me.styleOn) && findSpecial(c, Motion::QCF, 0, me.styleOn)->projectile;
    bool hasLowProjectile = findSpecial(c, Motion::QCB, 0, me.styleOn) && findSpecial(c, Motion::QCB, 0, me.styleOn)->projectile;
    if (dist > 4.5f) {
        if ((hasProjectile || hasLowProjectile) && r < 0.35f) {
            in.requestMove = findSpecial(c, hasProjectile ? Motion::QCF : Motion::QCB, rng_.range(0, 2), me.styleOn);
        } else if (r < 0.55f) { in.pressed |= BTN_DASH; in.dirX = relToAbs(1); }
        else { holdDir_ = 1; holdDirY_ = 0; holdFrames_ = 12 + rng_.range(0, 18); in.dirX = relToAbs(1); }
    } else if (dist > 2.3f) {
        if (r < 0.35f) { holdDir_ = 1; holdDirY_ = 0; holdFrames_ = 8 + rng_.range(0, 12); in.dirX = relToAbs(1); }
        else if (r < 0.5f) { in.dirY = 1; in.dirX = relToAbs(1); }           // jump in
        else if (r < 0.62f) { in.pressed |= BTN_DASH; in.dirX = relToAbs(1); }
        else if (r < 0.8f) { in.requestMove = pickAttack(me, opp, dist); }
        else if (r < 0.88f) { holdDir_ = -1; holdDirY_ = 0; holdFrames_ = 6 + rng_.range(0, 10); in.dirX = relToAbs(-1); }
        else { holdDir_ = 0; holdDirY_ = 0; holdFrames_ = 4 + rng_.range(0, 8); }
    } else {
        float aggression = 0.45f + 0.15f * difficulty_;
        if (r < aggression) { in.requestMove = pickAttack(me, opp, dist); }
        else if (r < aggression + 0.12f) { holdDir_ = -1; holdDirY_ = 0; holdFrames_ = 6 + rng_.range(0, 10); in.dirX = relToAbs(-1); }
        else if (r < aggression + 0.2f) { in.pressed |= BTN_DASH; in.dirY = rng_.chance(0.5f) ? 1 : -1; }   // sidestep
        else if (r < aggression + 0.28f) { in.dirY = 1; in.dirX = relToAbs(1); }
        else if (r < aggression + 0.36f) { holdDir_ = 0; holdDirY_ = -1; holdFrames_ = 6 + rng_.range(0, 10); in.dirY = -1; }
        else { holdDir_ = 1; holdDirY_ = 0; holdFrames_ = 3 + rng_.range(0, 6); in.dirX = relToAbs(1); }
    }
    return in;
}

} // namespace dc
