#include "game/fight.h"
#include <algorithm>
#include <cstring>

namespace dc {

namespace {
const uint32_t ATTACK_MASK = BTN_LIGHT | BTN_MEDIUM | BTN_HEAVY;
int popcount(uint32_t v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }
float hurtHeight(const Fighter& f) {
    if (!f.grounded()) return 1.5f;
    if (f.state == FState::Crouch || f.state == FState::CrouchBlock || f.state == FState::CrouchHit ||
        (f.state == FState::Attack && f.move && f.move->stance == Stance::Crouch) || f.state == FState::Sidestep)
        return 1.15f;
    if (f.isDown() || f.state == FState::KO) return 0.5f;
    return f.ch->height;
}
bool overlap(Vec3 ca, Vec3 sa, Vec3 cb, Vec3 sb) {
    return std::fabs(ca.x - cb.x) * 2 < sa.x + sb.x &&
           std::fabs(ca.y - cb.y) * 2 < sa.y + sb.y &&
           std::fabs(ca.z - cb.z) * 2 < sa.z + sb.z;
}
int kindPriority(MoveKind k) {
    switch (k) {
        case MoveKind::GHA: return 0; case MoveKind::HHA: return 1; case MoveKind::Throw: return 2;
        case MoveKind::Special: return 3; case MoveKind::Normal: return 4; case MoveKind::Taunt: return 5;
    }
    return 6;
}
} // namespace

void FightSim::emit(FightEvent::Type t, Vec3 pos, float strength, int player, const std::string& text) {
    FightEvent e; e.type = t; e.pos = pos; e.strength = strength; e.player = player; e.text = text;
    events_.push_back(e);
}

void FightSim::init(const Character* a, const Character* b, int roundsToWin) {
    f_[0] = Fighter(); f_[1] = Fighter();
    f_[0].ch = a; f_[1].ch = b;
    f_[0].index = 0; f_[1].index = 1;
    roundsToWin_ = roundsToWin;
    round_ = 1; matchWinner_ = -1; frame_ = 0;
    for (int i = 0; i < 2; ++i) { f_[i].hhg = 0; f_[i].roundsWon = 0; }
    startRound();
}

void FightSim::startRound() {
    for (int i = 0; i < 2; ++i) {
        Fighter& f = f_[i];
        float hhg = f.hhg; int wins = f.roundsWon; const Character* ch = f.ch;
        f = Fighter();
        f.ch = ch; f.index = i; f.hhg = hhg; f.roundsWon = wins;
        f.pos = Vec3(i == 0 ? -2.2f : 2.2f, 0, 0);
        f.facing = i == 0 ? 1 : -1;
        f.hp = f.hpShown = f.hpRedShown = ch->hp;
        f.styleGauge = 1.0f;
        f.state = FState::Intro;
    }
    timerFrames_ = ROUND_SECONDS * 60;
    phase_ = Phase::Intro; phaseTimer_ = 0;
    winner_ = -1; dramatic_ = false; timeScale_ = 1.0f;
    clash_ = ClashState();
    projectiles_.clear();
    emit(FightEvent::RoundStart, Vec3(), 1, 0, round_ >= 5 ? "FINAL ROUND" : ("ROUND " + std::to_string(round_)));
}

void FightSim::setState(Fighter& f, FState s, int len) {
    f.state = s; f.timer = 0; f.stateLen = len;
    if (s != FState::Attack) { f.move = nullptr; f.moveFrame = 0; f.standAttackBlend = 0; }
    if (s == FState::Idle || s == FState::Walk || s == FState::Crouch) { f.chainLevel = -1; }
}

// ------------------------------------------------------------------ input ----
void FightSim::updateInput(Fighter& f, const SimInput& in) {
    int rel = in.dirX * f.facing;
    int numpad = 5 + rel + 3 * in.dirY;
    f.dirHistory[f.histHead % 32] = numpad;
    f.histHead = (f.histHead + 1) % 32;
    f.wantsBlock = rel == -1;

    // double-tap dash
    f.tapTimer++;
    if (rel == 0) f.neutralSeen = true;
    else if (f.neutralSeen) {
        f.neutralSeen = false;
        if (rel == f.lastTapDir && f.tapTimer <= 12) f.dashRequest = rel;
        f.lastTapDir = rel; f.tapTimer = 0;
    }

    uint32_t atk = in.pressed & (ATTACK_MASK | BTN_TAUNT);
    if (atk) {
        if (f.pendingButtons == 0) f.pendingAge = 0;
        f.pendingButtons |= atk;
    }
    if (f.pendingButtons) { f.pendingAge++; if (f.pendingAge > 14) { f.pendingButtons = 0; f.pendingAge = 0; } }
}

bool FightSim::motionDetected(const Fighter& f, Motion m) const {
    if (m == Motion::None) return true;
    auto at = [&](int k) { return f.dirHistory[((f.histHead - 1 - k) % 32 + 32) % 32]; };
    auto matches = [](int v, int which) {
        switch (which) {
            case 2: return v == 2 || v == 1 || v == 3;
            case 3: return v == 3;
            case 1: return v == 1;
            case 6: return v == 6 || v == 9;
            case 4: return v == 4 || v == 7;
            case 36: return v == 3 || v == 6 || v == 9;
            case 14: return v == 1 || v == 4 || v == 7;
        }
        return false;
    };
    int seq[3]; int n = 3;
    switch (m) {
        case Motion::QCF: seq[0] = 2; seq[1] = 3; seq[2] = 6; break;
        case Motion::QCB: seq[0] = 2; seq[1] = 1; seq[2] = 4; break;
        case Motion::DP: seq[0] = 36; seq[1] = 2; seq[2] = 36; break;
        default: return true;
    }
    int k = 0;
    const int maxSpan = 18;
    for (int idx = n - 1; idx >= 0; --idx) {
        bool found = false;
        int limit = (idx == n - 1) ? 7 : maxSpan;
        while (k < limit) {
            if (matches(at(k), seq[idx])) { found = true; break; }
            ++k;
        }
        if (!found) return false;
        // skip the run of the same direction
        while (k < maxSpan && matches(at(k), seq[idx])) ++k;
    }
    return true;
}

void FightSim::beginMove(Fighter& f, const Move* m) {
    setState(f, FState::Attack);
    f.move = m; f.moveFrame = 0;
    std::memset(f.hitDone, 0, sizeof(f.hitDone));
    f.moveConnected = false; f.armorUsed = false;
    f.chainLevel = m->kind == MoveKind::Normal ? m->chainLevel : 3;
    if (!f.grounded()) f.airAttackUsed = true;
    if (m->hhgCost > 0) f.hhg = std::max(0.0f, f.hhg - 1000.0f * m->hhgCost);
    f.pendingButtons = 0; f.pendingAge = 0;
    if (m->kind == MoveKind::Special || m->kind == MoveKind::HHA || m->kind == MoveKind::GHA) {
        f.lastMoveName = m->name; f.lastMoveNameTimer = 80;
        emit(FightEvent::MoveName, f.pos, m->kind == MoveKind::GHA ? 3.0f : (m->kind == MoveKind::HHA ? 2.0f : 1.0f), f.index, m->name);
    }
    if (m->kind == MoveKind::HHA || m->kind == MoveKind::GHA) {
        shake_ = std::max(shake_, 0.4f);
    }
}

bool FightSim::tryFlashCancel(Fighter& f, uint32_t group) {
    if (popcount(group & ATTACK_MASK) < 3) return false;
    if (f.state != FState::Attack || !f.move) return false;
    if (f.hhg < 1000) return false;
    if (f.move->kind == MoveKind::GHA || f.move->kind == MoveKind::Throw || f.move->kind == MoveKind::Taunt) return false;
    if (f.moveFrame < 3) return false;
    f.hhg -= 1000;
    f.pendingButtons = 0;
    emit(FightEvent::FlashCancel, f.pos + Vec3(0, 1.0f, 0), 1, f.index, "FLASH CANCEL");
    setState(f, f.grounded() ? FState::Idle : FState::Jump);
    f.chainLevel = -1;
    return true;
}

bool FightSim::tryStartMove(Fighter& f, Fighter& o, uint32_t group, bool fromCancel) {
    const Character& c = *f.ch;
    bool air = !f.grounded();
    auto at = [&](int k) { return f.dirHistory[((f.histHead - 1 - k) % 32 + 32) % 32]; };
    int cur = at(0);
    bool holdingDown = cur == 1 || cur == 2 || cur == 3;
    Stance stance = air ? Stance::Air : (holdingDown ? Stance::Crouch : Stance::Ground);
    int nAtk = popcount(group & ATTACK_MASK);
    bool qcf = motionDetected(f, Motion::QCF);
    const Move* cur_move = (f.state == FState::Attack) ? f.move : nullptr;

    auto standOk = [&](const Move& m) {
        if (c.style != StyleType::Stand) return m.stand != StandUse::On;
        if (m.stand == StandUse::Any) return true;
        return (m.stand == StandUse::On) == f.styleOn;
    };

    // ---- Easy Beat: tapping Light repeatedly auto-chains L -> M -> H -> special
    if (fromCancel && cur_move && cur_move->kind == MoveKind::Normal && group == BTN_LIGHT && !air) {
        int want = cur_move->chainLevel + 1;
        if (want <= 2) {
            for (const Move& m : c.moves)
                if (m.kind == MoveKind::Normal && m.chainLevel == want && m.stance == cur_move->stance && standOk(m)) {
                    beginMove(f, &m); return true;
                }
        } else {
            for (const Move& m : c.moves)
                if (m.kind == MoveKind::Special && m.buttons == BTN_LIGHT && !m.commandThrow && standOk(m)) {
                    beginMove(f, &m); return true;
                }
        }
    }

    std::vector<const Move*> cands;
    for (const Move& m : c.moves) cands.push_back(&m);
    std::stable_sort(cands.begin(), cands.end(), [](const Move* a, const Move* b) {
        return kindPriority(a->kind) < kindPriority(b->kind);
    });

    uint32_t strongest = (group & BTN_HEAVY) ? BTN_HEAVY : ((group & BTN_MEDIUM) ? BTN_MEDIUM : BTN_LIGHT);

    for (const Move* mp : cands) {
        const Move& m = *mp;
        if (!standOk(m)) continue;
        bool ok = false;
        switch (m.kind) {
            case MoveKind::GHA:
                ok = nAtk == 3 && qcf && f.hhg >= 2000 && !air;
                break;
            case MoveKind::HHA:
                ok = nAtk == 2 && qcf && f.hhg >= 1000 && !air;
                break;
            case MoveKind::Throw:
                ok = nAtk == 2 && (group & BTN_LIGHT) && (group & BTN_MEDIUM) && !qcf && !air && !fromCancel;
                break;
            case MoveKind::Special:
                ok = (group & m.buttons) != 0 && nAtk == 1 && !air && motionDetected(f, m.motion);
                if (ok && fromCancel) ok = cur_move && cur_move->kind == MoveKind::Normal && cur_move->specialCancel;
                break;
            case MoveKind::Normal:
                ok = (m.buttons == strongest) && m.stance == stance && nAtk >= 1;
                if (ok && fromCancel) ok = cur_move && cur_move->kind == MoveKind::Normal && m.chainLevel > cur_move->chainLevel;
                if (ok && air && f.airAttackUsed) ok = false;
                break;
            case MoveKind::Taunt:
                ok = (group & BTN_TAUNT) && !air && !fromCancel;
                break;
        }
        if (!ok) continue;
        if (fromCancel && cur_move && (m.kind == MoveKind::HHA || m.kind == MoveKind::GHA)) {
            // Heat attacks cancel from normals and specials (not from throws / taunts)
            if (cur_move->kind == MoveKind::Throw || cur_move->kind == MoveKind::Taunt) continue;
        }
        beginMove(f, &m);
        return true;
    }
    return false;
}

// --------------------------------------------------------------- fighters ----
void FightSim::updateFacing(Fighter& f, const Fighter& o) {
    if (std::fabs(o.pos.x - f.pos.x) > 0.05f) f.facing = o.pos.x > f.pos.x ? 1 : -1;
}

void FightSim::applyPhysics(Fighter& f) {
    const Character& c = *f.ch;
    bool wasAir = !f.grounded();
    if (wasAir || f.vy > 0) {
        f.vy -= c.gravity;
        f.pos.y += f.vy;
        if (f.pos.y <= 0) {
            f.pos.y = 0; f.vy = 0;
            if (wasAir) {
                // landing
                if (f.state == FState::Jump || (f.state == FState::Attack && f.move && f.move->stance == Stance::Air)) {
                    setState(f, FState::Land, 4);
                    emit(FightEvent::Dust, f.pos, 0.6f, f.index);
                } else if (f.state == FState::AirHit) {
                    setState(f, FState::Knockdown, f.hardKD ? 52 : 34);
                    f.vx *= 0.3f;
                    emit(FightEvent::Land, f.pos, 1.0f, f.index);
                    shake_ = std::max(shake_, 0.25f);
                } else if (f.state == FState::KO) {
                    f.vx *= 0.2f;
                    emit(FightEvent::Land, f.pos, 1.0f, f.index);
                }
                f.airAttackUsed = false;
            }
        }
    }
    f.pos.x += f.vx;
    if (f.grounded()) f.vx *= 0.82f; else f.vx *= 0.995f;
    if (std::fabs(f.vx) < 0.001f) f.vx = 0;
    f.pos.x = clampf(f.pos.x, -STAGE_HALF, STAGE_HALF);
    // lateral (sidestep) motion
    f.pos.z = moveTowards(f.pos.z, f.sideTarget, 0.22f);
}

void FightSim::updateMovement(Fighter& f, Fighter& o, const SimInput& in) {
    const Character& c = *f.ch;
    int rel = in.dirX * f.facing;
    float spd = c.walkSpeed * f.effectiveSpeedMul();
    float bspd = c.backSpeed * f.effectiveSpeedMul();

    // style toggle
    if ((in.pressed & BTN_STYLE) && f.styleLock == 0) {
        if (c.style == StyleType::Stand) {
            f.styleOn = !f.styleOn; f.styleLock = 12;
            emit(FightEvent::Text, f.pos + Vec3(0, 2.2f, 0), 1, f.index, f.styleOn ? "STAND ON" : "STAND OFF");
        } else {
            if (f.styleOn) { f.styleOn = false; f.styleLock = 12; emit(FightEvent::Text, f.pos + Vec3(0, 2.2f, 0), 1, f.index, "MODE OFF"); }
            else if (f.styleGauge > 0.25f) { f.styleOn = true; f.styleLock = 12; emit(FightEvent::Text, f.pos + Vec3(0, 2.2f, 0), 1, f.index, c.styleName); }
        }
    }

    // dash / sidestep
    int dashDir = f.dashRequest;
    f.dashRequest = 0;
    if (in.pressed & BTN_DASH) {
        if (in.dirY != 0) {
            setState(f, FState::Sidestep, 22);
            f.sideTarget = in.dirY > 0 ? -1.5f : 1.5f;
            f.invulStrikes = 14;
            f.invulThrows = 16;
            emit(FightEvent::Dust, f.pos, 0.5f, f.index);
            return;
        }
        dashDir = rel >= 0 ? 1 : -1;
    }
    if (dashDir == 1) { setState(f, FState::Dash, 14); f.vx = f.facing * 0.17f * f.effectiveSpeedMul(); emit(FightEvent::Dust, f.pos, 0.5f, f.index); return; }
    if (dashDir == -1) { setState(f, FState::Backdash, 18); f.vx = -f.facing * 0.14f; f.invulStrikes = 8; f.invulThrows = 10; emit(FightEvent::Dust, f.pos, 0.5f, f.index); return; }

    // jump
    if (in.dirY > 0) {
        setState(f, FState::Jump);
        f.vy = c.jumpVel;
        f.vx = rel * (rel > 0 ? spd : bspd) * 1.15f * f.facing;
        f.airAttackUsed = false;
        emit(FightEvent::Dust, f.pos, 0.4f, f.index);
        return;
    }
    // crouch
    if (in.dirY < 0) { if (f.state != FState::Crouch) setState(f, FState::Crouch); return; }
    // walk
    if (rel != 0) {
        if (f.state != FState::Walk) setState(f, FState::Walk);
        f.pos.x += rel * (rel > 0 ? spd : bspd) * f.facing;
        f.pos.x = clampf(f.pos.x, -STAGE_HALF, STAGE_HALF);
        return;
    }
    if (f.state != FState::Idle) setState(f, FState::Idle);
}

void FightSim::updateFighter(Fighter& f, Fighter& o, const SimInput& in) {
    const Character& c = *f.ch;
    if (f.styleLock > 0) f.styleLock--;
    if (f.invulStrikes > 0) f.invulStrikes--;
    if (f.invulThrows > 0) f.invulThrows--;
    if (f.comboShow > 0) f.comboShow--;
    if (f.lastMoveNameTimer > 0) f.lastMoveNameTimer--;
    f.flash = std::max(0.0f, f.flash - 0.08f);

    // style gauge
    if (c.style == StyleType::Stand) {
        if (f.state != FState::StyleBreak) f.styleGauge = std::min(1.0f, f.styleGauge + (f.styleOn ? 0.0004f : 0.0012f));
    } else {
        if (f.styleOn) {
            f.styleGauge -= 0.0011f;
            if (f.styleGauge <= 0) { f.styleGauge = 0; f.styleOn = false; f.styleLock = 60; emit(FightEvent::Text, f.pos + Vec3(0, 2.2f, 0), 1, f.index, "MODE EXHAUSTED"); }
        } else f.styleGauge = std::min(1.0f, f.styleGauge + 0.0007f);
    }

    if (f.hitstop > 0) { f.hitstop--; return; }
    f.timer++;

    bool ready = f.pendingButtons != 0 && (popcount(f.pendingButtons & ATTACK_MASK) >= 2 || f.pendingAge >= 2 || (f.pendingButtons & BTN_TAUNT));
    uint32_t group = ready ? f.pendingButtons : 0;
    if (in.requestMove) group = 0;

    switch (f.state) {
    case FState::Intro:
    case FState::Win:
    case FState::KO:
        break;

    case FState::Idle:
    case FState::Walk:
    case FState::Crouch: {
        updateFacing(f, o);
        if (in.requestMove) {
            const Move& m = *in.requestMove;
            bool ok = (m.stance != Stance::Air) && f.hhg >= 1000 * m.hhgCost;
            if (c.style == StyleType::Stand && m.stand != StandUse::Any && ((m.stand == StandUse::On) != f.styleOn)) ok = false;
            if (ok) { beginMove(f, &m); break; }
        }
        if (group && tryStartMove(f, o, group, false)) break;
        updateMovement(f, o, in);
        break;
    }
    case FState::Land:
        updateFacing(f, o);
        if (f.timer >= f.stateLen) setState(f, FState::Idle);
        break;

    case FState::Jump: {
        if (in.requestMove && in.requestMove->stance == Stance::Air && !f.airAttackUsed) { beginMove(f, in.requestMove); break; }
        if (group && tryStartMove(f, o, group, false)) break;
        break;
    }
    case FState::Dash:
    case FState::Backdash:
        if (f.timer >= f.stateLen) setState(f, FState::Idle);
        else if (f.state == FState::Dash && f.timer >= 6 && group && tryStartMove(f, o, group, false)) break;
        break;

    case FState::Sidestep:
        if (f.timer >= 10) f.sideTarget = 0;
        if (f.timer >= f.stateLen) { setState(f, FState::Idle); updateFacing(f, o); }
        break;

    case FState::Attack: {
        const Move* m = f.move;
        f.moveFrame++;
        int total = m->totalFrames();
        // movement during the move
        if (f.moveFrame >= m->moveStart && f.moveFrame <= m->moveEnd && m->moveSpeed != 0) {
            // rush moves stop at the opponent's body
            float dx = (o.pos.x - f.pos.x) * f.facing;
            if (!(m->commandThrow && dx < 0.9f)) {
                f.pos.x += f.facing * m->moveSpeed * f.effectiveSpeedMul();
                f.pos.x = clampf(f.pos.x, -STAGE_HALF, STAGE_HALF);
            }
        }
        if (f.moveFrame == std::max(1, m->moveStart) && m->jumpVel > 0) f.vy = m->jumpVel;
        if (f.moveFrame >= m->invulStart && f.moveFrame <= m->invulEnd) f.invulStrikes = std::max(f.invulStrikes, 1);
        if (m->kind == MoveKind::Taunt && f.moveFrame == m->startup) {
            if (o.isDown() || o.state == FState::StyleBreak) {
                o.hhg = std::max(0.0f, o.hhg - 400.0f);
                emit(FightEvent::Text, f.pos + Vec3(0, 2.3f, 0), 1, f.index, "TAUNT!");
            }
        }
        // projectile spawn
        if (m->projectile && !m->hits.empty() && f.moveFrame == m->hits[0].start) {
            Projectile p;
            p.pos = Vec3(f.pos.x + f.facing * m->hits[0].boxPos.x, f.pos.y + m->hits[0].boxPos.y, f.pos.z);
            p.vx = f.facing * m->projectileSpeed;
            p.owner = f.index; p.life = 110; p.hit = m->hits[0]; p.color = c.styleColor;
            projectiles_.push_back(p);
            emit(FightEvent::Projectile, p.pos, 1, f.index);
        }
        // Stand "out front" blend for presentation
        f.standAttackBlend = m->standAttack ? std::min(1.0f, f.standAttackBlend + 0.2f) : std::max(0.0f, f.standAttackBlend - 0.2f);

        // cancels
        bool connectedWindow = f.moveConnected && f.moveFrame >= m->startup && f.moveFrame <= m->startup + m->active + 8;
        if (in.requestMove && connectedWindow) {
            const Move& r = *in.requestMove;
            bool ok = false;
            if (r.kind == MoveKind::Normal && m->kind == MoveKind::Normal && r.chainLevel > m->chainLevel && r.stance != Stance::Air) ok = true;
            if (r.kind == MoveKind::Special && m->kind == MoveKind::Normal && m->specialCancel && !r.commandThrow) ok = true;
            if ((r.kind == MoveKind::HHA || r.kind == MoveKind::GHA) && (m->kind == MoveKind::Normal || m->kind == MoveKind::Special) && f.hhg >= 1000 * r.hhgCost) ok = true;
            if (ok && c.style == StyleType::Stand && r.stand != StandUse::Any && ((r.stand == StandUse::On) != f.styleOn)) ok = false;
            if (ok) { beginMove(f, &r); break; }
        }
        if (group) {
            if (connectedWindow && tryStartMove(f, o, group, true)) break;
            if (tryFlashCancel(f, group)) break;
        }
        if (f.moveFrame >= total) {
            if (!f.grounded()) setState(f, FState::Jump);
            else { setState(f, FState::Idle); updateFacing(f, o); }
        }
        break;
    }
    case FState::Block:
    case FState::CrouchBlock:
        // Stylish Evade: sidestep out of blockstun
        if ((in.pressed & BTN_DASH) && in.dirY != 0) {
            setState(f, FState::Sidestep, 22);
            f.sideTarget = in.dirY > 0 ? -1.5f : 1.5f;
            f.invulStrikes = 14; f.invulThrows = 16;
            emit(FightEvent::Text, f.pos + Vec3(0, 2.2f, 0), 1, f.index, "STYLISH EVADE");
            break;
        }
        if (f.timer >= f.stateLen) setState(f, in.dirY < 0 ? FState::Crouch : FState::Idle);
        break;

    case FState::Hit:
    case FState::CrouchHit:
        if (f.timer >= f.stateLen) { setState(f, FState::Idle); f.inCombo = false; }
        break;
    case FState::AirHit:
        break; // physics handles landing
    case FState::Knockdown:
        if (f.timer >= f.stateLen) { setState(f, FState::GetUp, 14); f.invulStrikes = 16; f.invulThrows = 16; f.inCombo = false; f.hardKD = false; }
        break;
    case FState::GetUp:
        if (f.timer >= f.stateLen) { setState(f, FState::Idle); updateFacing(f, o); }
        break;
    case FState::Thrown:
        // held in front of the thrower
        f.pos.x = clampf(o.pos.x + o.facing * 0.85f, -STAGE_HALF, STAGE_HALF);
        f.pos.y = 0.15f + 0.25f * std::sin(PI * std::min(1.0f, (float)f.timer / (float)f.stateLen));
        if (f.timer >= f.stateLen) {
            f.pos.y = 0.3f;
            setState(f, FState::AirHit);
            f.vy = 0.10f; f.vx = o.facing * 0.18f;
            f.hardKD = true;
        }
        break;
    case FState::StyleBreak:
        if (f.timer >= f.stateLen) { setState(f, FState::Idle); f.inCombo = false; }
        break;
    case FState::Taunt:
    case FState::Clash:
        break;
    }

    // walking into the opponent's attack range shows guard pose (proximity guard)
    f.proxGuard = (f.wantsBlock && (o.attacking()) && std::fabs(o.pos.x - f.pos.x) < 2.6f) ? 1 : 0;
}

// ----------------------------------------------------------------- hits -----
bool FightSim::checkHit(Fighter& att, Fighter& def, const HitWindow& w, const Move* move, Vec3 box, Vec3 size, bool fromProjectile) {
    if (def.state == FState::KO || def.state == FState::Win || def.state == FState::Intro) return false;
    if (def.isDown()) return false;
    if (def.state == FState::Thrown) return false;
    float h = hurtHeight(def);
    Vec3 hurtC(def.pos.x, def.pos.y + h * 0.5f, def.pos.z);
    Vec3 hurtS(0.75f, h, 0.8f);
    if (!overlap(box, size, hurtC, hurtS)) return false;

    bool isThrow = move && move->commandThrow;
    if (isThrow) {
        if (!def.grounded() || def.inHitstun() || def.invulThrows > 0 || def.state == FState::Sidestep || def.state == FState::Backdash || def.state == FState::Jump) return false;
        // throw tech
        if (move->kind == MoveKind::Throw && def.state == FState::Attack && def.move && def.move->kind == MoveKind::Throw && def.moveFrame <= 10) {
            setState(att, FState::Idle); setState(def, FState::Idle);
            att.vx = -att.facing * 0.14f; def.vx = -def.facing * 0.14f;
            emit(FightEvent::Tech, (att.pos + def.pos) * 0.5f + Vec3(0, 1.2f, 0), 1, att.index, "THROW TECH");
            return true;
        }
    } else {
        if (def.invulStrikes > 0) return false;
    }
    // armor
    if (!isThrow && def.state == FState::Attack && def.move && def.move->armor && !def.armorUsed && def.moveFrame < def.move->startup) {
        def.armorUsed = true;
        float dmg = w.damage * 0.5f;
        def.hp -= dmg; att.hhg = std::min(3000.0f, att.hhg + dmg);
        def.flash = 1.0f; att.hitstop = 4; def.hitstop = 4;
        emit(FightEvent::Block, Vec3(box.x, box.y, box.z), 1.2f, att.index, "ARMOR");
        att.moveConnected = true;
        return true;
    }
    // block check
    bool canBlock = !w.standHit || true;
    bool blocking = false;
    if (canBlock && !isThrow && !(move && move->unblockable) && def.grounded()) {
        bool guardState = def.state == FState::Block || def.state == FState::CrouchBlock;
        bool neutralGuard = (def.state == FState::Idle || def.state == FState::Walk || def.state == FState::Crouch || def.state == FState::Land) && def.wantsBlock;
        if (guardState || neutralGuard) {
            bool crouching = def.state == FState::Crouch || def.state == FState::CrouchBlock ||
                             (def.state == FState::Block ? false : (def.dirHistory[((def.histHead - 1) % 32 + 32) % 32] <= 3));
            if (w.low && !crouching) blocking = false;
            else if (w.overhead && crouching) blocking = false;
            else blocking = true;
            if (blocking && def.state == FState::Block && w.low) blocking = false;
            if (blocking && def.state == FState::CrouchBlock && w.overhead) blocking = false;
            if (blocking) { applyBlock(att, def, w, move, box); return true; }
        }
    }
    applyHit(att, def, w, move, box, fromProjectile);
    return true;
}

void FightSim::applyBlock(Fighter& att, Fighter& def, const HitWindow& w, const Move* move, Vec3 at) {
    bool crouch = def.state == FState::CrouchBlock || def.state == FState::Crouch ||
                  (def.state != FState::Block && def.dirHistory[((def.histHead - 1) % 32 + 32) % 32] <= 3);
    setState(def, crouch ? FState::CrouchBlock : FState::Block, w.blockstun);
    def.vx = att.facing * w.knockX * 0.7f;
    float dmg = w.damage * att.effectiveDamageMul();
    if (w.chip > 0) def.hp = std::max(1.0f, def.hp - dmg * w.chip);
    att.hhg = std::min(3000.0f, att.hhg + dmg * 0.3f);
    def.hhg = std::min(3000.0f, def.hhg + dmg * 0.25f);
    att.hitstop = 3; def.hitstop = 3;
    att.moveConnected = true;
    if (std::fabs(def.pos.x) >= STAGE_HALF - 0.01f) att.vx -= att.facing * w.knockX * 0.6f;
    emit(FightEvent::Block, at, 0.8f, att.index);
    def.inCombo = false;
    att.comboHits = 0;
}

void FightSim::applyHit(Fighter& att, Fighter& def, const HitWindow& w, const Move* move, Vec3 at, bool fromProjectile) {
    bool isThrow = move && move->commandThrow;
    bool wasInCombo = def.inHitstun() || def.inCombo;
    if (wasInCombo) att.comboHits++; else { att.comboHits = 1; att.comboDamage = 0; }
    float scale = isThrow ? 1.0f : std::max(0.3f, 1.0f - 0.08f * (att.comboHits - 1));
    float dmg = w.damage * scale * att.effectiveDamageMul();
    def.hp -= dmg;
    att.comboDamage += dmg;
    att.comboShow = 100;
    def.inCombo = true;
    att.hhg = std::min(3000.0f, att.hhg + dmg * 1.1f);
    def.hhg = std::min(3000.0f, def.hhg + dmg * 0.55f);
    att.hitstop = w.hitstop; def.hitstop = w.hitstop;
    def.flash = 1.0f;
    att.moveConnected = true;
    shake_ = std::max(shake_, std::min(1.0f, dmg / 120.0f));
    emit(FightEvent::Hit, at, std::min(2.5f, 0.5f + dmg / 60.0f), att.index);
    if (isThrow) emit(FightEvent::Throw, at, 1, att.index);

    // Stand gauge: the Stand takes the hit
    bool styleBreak = false;
    if (def.ch->style == StyleType::Stand && def.styleOn) {
        def.styleGauge -= dmg / 320.0f;
        if (def.styleGauge <= 0) { def.styleGauge = 0; def.styleOn = false; def.styleLock = 120; styleBreak = true; }
    }
    // corner push
    if (std::fabs(def.pos.x) >= STAGE_HALF - 0.01f && !isThrow) att.vx -= att.facing * w.knockX * 0.9f;

    if (def.hp <= 0) {
        def.hp = 0;
        return; // KO handled in checkRoundEnd
    }
    if (styleBreak) {
        setState(def, FState::StyleBreak, 110);
        def.vx = att.facing * w.knockX;
        emit(FightEvent::StyleBreak, def.pos + Vec3(0, 1.5f, 0), 1.5f, def.index, "STAND BREAK");
        shake_ = 1.0f;
        return;
    }
    if (isThrow) {
        setState(def, FState::Thrown, 22);
        def.hardKD = true;
        return;
    }
    bool airborne = !def.grounded() || def.state == FState::AirHit;
    if (w.launcher || airborne) {
        float juggleDecay = 0.006f * std::max(0, att.comboHits - 2);
        float vy = w.launcher ? std::max(w.knockY, 0.16f) : std::max(0.08f, w.knockY + 0.06f);
        setState(def, FState::AirHit);
        def.vy = std::max(0.05f, vy - juggleDecay);
        def.vx = att.facing * std::max(w.knockX, 0.04f);
        def.hardKD = w.hardKnockdown;
        if (def.pos.y <= 0) def.pos.y = 0.01f;
    } else if (w.knockdown) {
        setState(def, FState::AirHit);
        def.vy = std::max(w.knockY, 0.09f);
        def.vx = att.facing * w.knockX;
        def.hardKD = w.hardKnockdown;
        def.pos.y = 0.01f;
    } else {
        bool crouch = def.state == FState::Crouch || def.state == FState::CrouchHit || def.state == FState::CrouchBlock ||
                      (def.state == FState::Attack && def.move && def.move->stance == Stance::Crouch);
        setState(def, crouch ? FState::CrouchHit : FState::Hit, w.hitstun);
        def.vx = att.facing * w.knockX;
    }
}

void FightSim::resolveHits() {
    // Rush Mode clash: both rush attacks active and reaching each other at the same time
    for (int i = 0; i < 2; ++i) {
        Fighter& a = f_[i]; Fighter& d = f_[1 - i];
        if (a.state != FState::Attack || !a.move || a.hitstop > 0) continue;
        const Move& m = *a.move;
        if (m.projectile) continue;
        for (size_t k = 0; k < m.hits.size() && k < 24; ++k) {
            const HitWindow& w = m.hits[k];
            if (a.moveFrame < w.start || a.moveFrame > w.end || a.hitDone[k]) continue;
            Vec3 box(a.pos.x + a.facing * w.boxPos.x, a.pos.y + w.boxPos.y, a.pos.z + w.boxPos.z);
            // clash detection
            if (m.rush && d.state == FState::Attack && d.move && d.move->rush && d.hitstop == 0 && !clash_.active) {
                bool dActive = d.moveFrame >= d.move->startup - 6 && d.moveFrame <= d.move->startup + d.move->active;
                if (dActive && std::fabs(a.pos.x - d.pos.x) < 3.4f && std::fabs(a.pos.z - d.pos.z) < 0.8f) { startClash(); return; }
            }
            if (checkHit(a, d, w, &m, box, w.boxSize, false)) a.hitDone[k] = true;
        }
    }
}

void FightSim::resolveProjectiles() {
    for (Projectile& p : projectiles_) {
        if (!p.alive) continue;
        p.pos.x += p.vx;
        p.spin += 0.3f;
        if (--p.life <= 0 || std::fabs(p.pos.x) > STAGE_HALF + 2.0f) { p.alive = false; continue; }
        Fighter& att = f_[p.owner]; Fighter& def = f_[1 - p.owner];
        if (checkHit(att, def, p.hit, nullptr, p.pos, p.hit.boxSize, true)) {
            p.alive = false;
            emit(FightEvent::Spark, p.pos, 1.0f, p.owner);
        }
    }
    // projectile vs projectile
    for (size_t i = 0; i < projectiles_.size(); ++i)
        for (size_t j = i + 1; j < projectiles_.size(); ++j) {
            Projectile& a = projectiles_[i]; Projectile& b = projectiles_[j];
            if (!a.alive || !b.alive || a.owner == b.owner) continue;
            if (overlap(a.pos, a.hit.boxSize, b.pos, b.hit.boxSize)) {
                a.alive = b.alive = false;
                emit(FightEvent::Spark, (a.pos + b.pos) * 0.5f, 1.5f, 0);
            }
        }
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(), [](const Projectile& p) { return !p.alive; }), projectiles_.end());
}

void FightSim::pushboxes() {
    Fighter& a = f_[0]; Fighter& b = f_[1];
    if (a.state == FState::Thrown || b.state == FState::Thrown) return;
    if (a.isDown() || b.isDown()) return;
    bool aAir = a.pos.y > 0.6f, bAir = b.pos.y > 0.6f;
    if (aAir != bAir) return; // crossups allowed
    if (std::fabs(a.pos.z - b.pos.z) > 0.9f) return;
    float minDist = 0.85f;
    float dx = b.pos.x - a.pos.x;
    if (std::fabs(dx) < minDist) {
        float push = (minDist - std::fabs(dx)) * 0.5f;
        float dir = dx >= 0 ? 1.0f : -1.0f;
        if (dx == 0) dir = a.facing >= 0 ? 1.0f : -1.0f;
        a.pos.x -= dir * push; b.pos.x += dir * push;
        // wall: transfer push
        if (a.pos.x < -STAGE_HALF) { b.pos.x += -STAGE_HALF - a.pos.x; a.pos.x = -STAGE_HALF; }
        if (a.pos.x > STAGE_HALF) { b.pos.x -= a.pos.x - STAGE_HALF; a.pos.x = STAGE_HALF; }
        if (b.pos.x < -STAGE_HALF) { a.pos.x += -STAGE_HALF - b.pos.x; b.pos.x = -STAGE_HALF; }
        if (b.pos.x > STAGE_HALF) { a.pos.x -= b.pos.x - STAGE_HALF; b.pos.x = STAGE_HALF; }
    }
}

// ---------------------------------------------------------------- clash -----
void FightSim::startClash() {
    clash_ = ClashState();
    clash_.active = true; clash_.timer = 0;
    for (int i = 0; i < 2; ++i) {
        setState(f_[i], FState::Clash, 150);
        f_[i].hitstop = 0; f_[i].vx = 0;
    }
    // line them up
    float mid = clampf((f_[0].pos.x + f_[1].pos.x) * 0.5f, -STAGE_HALF + 1.5f, STAGE_HALF - 1.5f);
    int left = f_[0].pos.x <= f_[1].pos.x ? 0 : 1;
    f_[left].pos.x = mid - 1.4f; f_[1 - left].pos.x = mid + 1.4f;
    f_[left].facing = 1; f_[1 - left].facing = -1;
    f_[0].pos.z = f_[1].pos.z = 0; f_[0].sideTarget = f_[1].sideTarget = 0;
    emit(FightEvent::Clash, Vec3(mid, 1.5f, 0), 1, 0, "RUSH MODE");
    shake_ = 0.8f;
}

void FightSim::updateClash(const SimInput& inA, const SimInput& inB) {
    clash_.timer++;
    const SimInput* ins[2] = {&inA, &inB};
    for (int i = 0; i < 2; ++i) clash_.presses[i] += popcount(ins[i]->pressed & ATTACK_MASK);
    if (clash_.timer % 6 == 0) {
        Vec3 mid = (f_[0].pos + f_[1].pos) * 0.5f + Vec3(0, 1.2f + rng_.uniform(-0.3f, 0.5f), rng_.uniform(-0.3f, 0.3f));
        emit(FightEvent::Spark, mid, 1.0f, clash_.timer % 12 == 0 ? 0 : 1);
        shake_ = std::max(shake_, 0.3f);
    }
    if (clash_.timer >= 150) {
        int w = clash_.presses[0] == clash_.presses[1] ? rng_.range(0, 1) : (clash_.presses[0] > clash_.presses[1] ? 0 : 1);
        Fighter& win = f_[w]; Fighter& lose = f_[1 - w];
        clash_.active = false;
        setState(win, FState::Idle);
        win.hhg = std::min(3000.0f, win.hhg + 500.0f);
        lose.inCombo = false;
        win.comboHits = 1;
        float dmg = 160.0f;
        lose.hp -= dmg;
        lose.flash = 1.0f;
        emit(FightEvent::Hit, lose.pos + Vec3(0, 1.3f, 0), 2.2f, w);
        emit(FightEvent::Text, lose.pos + Vec3(0, 2.4f, 0), 1.5f, w, win.ch->name + " WINS THE RUSH");
        shake_ = 1.0f;
        if (lose.hp <= 0) { lose.hp = 0; }
        else {
            setState(lose, FState::AirHit);
            lose.vy = 0.2f; lose.vx = win.facing * 0.22f; lose.hardKD = true; lose.pos.y = 0.01f;
        }
    }
}

// ---------------------------------------------------------------- rounds ----
void FightSim::checkRoundEnd() {
    int ko = -1;
    if (f_[0].hp <= 0 && f_[1].hp <= 0) ko = 2;
    else if (f_[0].hp <= 0) ko = 0;
    else if (f_[1].hp <= 0) ko = 1;
    bool timeOut = timerFrames_ <= 0;
    if (ko == -1 && !timeOut) return;

    phase_ = Phase::KO; phaseTimer_ = 0;
    clash_.active = false;
    if (ko == 2) winner_ = -1;
    else if (ko >= 0) winner_ = 1 - ko;
    else winner_ = f_[0].hp == f_[1].hp ? -1 : (f_[0].hp > f_[1].hp ? 0 : 1);

    for (int i = 0; i < 2; ++i) {
        Fighter& f = f_[i];
        bool lost = (ko == 2) || (ko == i);
        if (lost) {
            const Fighter& o = f_[1 - i];
            setState(f, FState::KO);
            f.vy = 0.14f; f.vx = o.facing * 0.10f;
            if (f.pos.y <= 0) f.pos.y = 0.01f;
            if (o.state == FState::Attack && o.move && (o.move->kind == MoveKind::HHA || o.move->kind == MoveKind::GHA)) dramatic_ = true;
        }
    }
    if (winner_ >= 0) f_[winner_].roundsWon++;
    emit(FightEvent::KO, Vec3(0, 1.5f, 0), dramatic_ ? 2.0f : 1.0f, winner_, ko >= 0 ? (dramatic_ ? "DRAMATIC FINISH" : "K.O.") : (ko == 2 ? "DOUBLE K.O." : "TIME OVER"));
    shake_ = 1.0f;
}

void FightSim::step(const SimInput& inA, const SimInput& inB) {
    events_.clear();
    frame_++;
    shake_ *= 0.86f;
    if (shake_ < 0.01f) shake_ = 0;
    const SimInput* ins[2] = {&inA, &inB};

    for (int i = 0; i < 2; ++i) {
        Fighter& f = f_[i];
        f.hpShown = moveTowards(f.hpShown, f.hp, 12.0f);
        if (f.hpRedShown < f.hp) f.hpRedShown = f.hp;
        f.hpRedShown = moveTowards(f.hpRedShown, f.hp, f.inCombo || f.inHitstun() ? 0.0f : 3.0f);
    }

    switch (phase_) {
    case Phase::Intro:
        phaseTimer_++;
        if (phaseTimer_ == 150) {
            phase_ = Phase::Fight; phaseTimer_ = 0;
            for (int i = 0; i < 2; ++i) setState(f_[i], FState::Idle);
            emit(FightEvent::Text, Vec3(0, 2, 0), 2.0f, -1, "FIGHT");
        }
        break;
    case Phase::Fight: {
        timeScale_ = 1.0f;
        if (timerFrames_ > 0) timerFrames_--;
        for (int i = 0; i < 2; ++i) updateInput(f_[i], *ins[i]);
        if (clash_.active) {
            updateClash(inA, inB);
        } else {
            for (int i = 0; i < 2; ++i) if (f_[i].state == FState::Attack && f_[i].move == nullptr) setState(f_[i], FState::Idle);
            for (int i = 0; i < 2; ++i) updateFighter(f_[i], f_[1 - i], *ins[i]);
            for (int i = 0; i < 2; ++i) if (f_[i].hitstop == 0) applyPhysics(f_[i]);
            pushboxes();
            resolveHits();
            resolveProjectiles();
        }
        checkRoundEnd();
        break;
    }
    case Phase::KO:
        phaseTimer_++;
        timeScale_ = phaseTimer_ < 50 ? (dramatic_ ? 0.18f : 0.3f) : 1.0f;
        for (int i = 0; i < 2; ++i) {
            Fighter& f = f_[i];
            if (f.hitstop > 0) f.hitstop = 0;
            if (f.state == FState::KO || f.state == FState::AirHit || f.state == FState::Thrown) {
                if (f.state == FState::Thrown) { setState(f, FState::KO); f.vy = 0.12f; }
                if (f.state == FState::AirHit && f.hp <= 0) setState(f, FState::KO);
                applyPhysics(f);
            } else if (f.state == FState::Attack && f.move) {
                f.moveFrame++;
                f.standAttackBlend = std::max(0.0f, f.standAttackBlend - 0.1f);
                if (f.moveFrame >= f.move->totalFrames()) setState(f, FState::Idle);
                applyPhysics(f);
            } else {
                applyPhysics(f);
                if (f.state != FState::KO && f.state != FState::Win && f.state != FState::Idle && f.state != FState::Knockdown && f.timer++ > 20) setState(f, FState::Idle);
            }
        }
        for (Projectile& p : projectiles_) { p.pos.x += p.vx; p.spin += 0.3f; if (--p.life <= 0) p.alive = false; }
        projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(), [](const Projectile& p) { return !p.alive; }), projectiles_.end());
        if (phaseTimer_ >= 110) {
            phase_ = Phase::RoundOver; phaseTimer_ = 0;
            if (winner_ >= 0 && f_[winner_].state != FState::KO) setState(f_[winner_], FState::Win);
            bool over = (winner_ >= 0 && f_[winner_].roundsWon >= roundsToWin_) || round_ >= 5;
            if (over) {
                matchWinner_ = winner_;
                if (matchWinner_ < 0) matchWinner_ = f_[0].roundsWon >= f_[1].roundsWon ? 0 : 1;
            }
        }
        break;
    case Phase::RoundOver:
        phaseTimer_++;
        timeScale_ = 1.0f;
        for (int i = 0; i < 2; ++i) applyPhysics(f_[i]);
        if (phaseTimer_ >= 140) {
            if (matchWinner_ >= 0) { phase_ = Phase::MatchOver; phaseTimer_ = 0; }
            else { round_++; startRound(); }
        }
        break;
    case Phase::MatchOver:
        phaseTimer_++;
        for (int i = 0; i < 2; ++i) applyPhysics(f_[i]);
        break;
    }
}

} // namespace dc
