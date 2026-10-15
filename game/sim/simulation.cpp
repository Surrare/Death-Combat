#include "sim/simulation.h"

#include "sim/boxes.h"
#include "sim/collision.h"
#include "sim/command_interpreter.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

namespace dc::sim {
namespace {

void setAction(FighterState& fighter, Action action) noexcept {
    if (fighter.action != action) {
        fighter.action = action;
        fighter.actionFrame = 0;
    }
}

bool isFreeToMove(const FighterState& fighter) noexcept {
    switch (fighter.action) {
    case Action::Idle:
    case Action::WalkForward:
    case Action::WalkBack:
    case Action::Crouch:
        return true;
    default:
        return false;
    }
}

void resetFighterForRound(FighterState& fighter, const CharacterData& character, std::int32_t x,
                          std::int32_t facing) noexcept {
    const std::int32_t roundsWon = fighter.roundsWon;
    const InputBuffer input = fighter.input;
    fighter = FighterState{};
    fighter.roundsWon = roundsWon;
    fighter.input = input;
    fighter.position = {x, 0};
    fighter.facing = facing;
    fighter.health = character.stats.health;
}

void startRound(MatchState& state, const MatchData& data, std::int32_t number) noexcept {
    const std::int32_t halfDistance = data.rules.startDistance / 2;
    resetFighterForRound(state.fighters[0], data.characters[0], -halfDistance, 1);
    resetFighterForRound(state.fighters[1], data.characters[1], halfDistance, -1);
    state.projectiles.clear();
    state.hitstop = 0;
    state.round = RoundState{number, RoundPhase::Intro, 0, data.rules.roundSeconds * ticksPerSecond};
}

void enterPhase(RoundState& round, RoundPhase phase) noexcept {
    round.phase = phase;
    round.phaseFrame = 0;
}

PlayerIndex winnerByHealth(const MatchState& state) noexcept {
    // TODO(§4.7): ничья по здоровью и двойной KO засчитывают раунд обоим.
    return state.fighters[0].health >= state.fighters[1].health ? PlayerIndex::P1 : PlayerIndex::P2;
}

// --- Шаг 1: ввод -------------------------------------------------------------------------------
void bufferInputs(MatchState& state, const StepInput& input) noexcept {
    for (std::size_t i = 0; i < state.fighters.size(); ++i) {
        state.fighters[i].input.push(input.players[i]);
    }
}

// --- Шаг 3: решения ----------------------------------------------------------------------------
void decideAction(FighterState& fighter, const CharacterData& character, bool canAct) noexcept {
    if (!isFreeToMove(fighter)) {
        return;
    }
    if (!canAct) {
        setAction(fighter, Action::Idle);
        fighter.velocity = {};
        return;
    }

    if (const MoveId move = findCommand(fighter.input, character, fighter.facing); move != MoveId::None) {
        // TODO(§4.5): начать приём — Action::Attack, fighter.move = move, actionFrame = 0, событие MoveStarted.
    }
    // TODO(§4.7): блок отдельной кнопкой (Button::Block), броски (1+3).

    const std::uint8_t direction = toRelativeDirection(fighter.input.at(0).direction, fighter.facing);
    if (isUp(direction)) {
        setAction(fighter, Action::JumpSquat);
        fighter.velocity = {};
        return;
    }
    if (isDown(direction)) {
        setAction(fighter, Action::Crouch);
        fighter.velocity = {};
        return;
    }

    const int towards = horizontalOf(direction);  // +1 вперёд, -1 назад
    if (towards > 0) {
        setAction(fighter, Action::WalkForward);
        fighter.velocity = {fighter.facing * character.stats.walkForwardSpeed, 0};
    } else if (towards < 0) {
        setAction(fighter, Action::WalkBack);
        fighter.velocity = {-fighter.facing * character.stats.walkBackSpeed, 0};
    } else {
        setAction(fighter, Action::Idle);
        fighter.velocity = {};
    }
}

// --- Шаг 4: продвижение действий ---------------------------------------------------------------
void advanceAction(FighterState& fighter, const CharacterData& character) noexcept {
    ++fighter.actionFrame;
    switch (fighter.action) {
    case Action::JumpSquat:
        if (fighter.actionFrame >= character.stats.jumpSquatFrames) {
            const std::uint8_t direction = toRelativeDirection(fighter.input.at(0).direction, fighter.facing);
            const std::int32_t horizontal = horizontalOf(direction) * fighter.facing;
            fighter.velocity = {horizontal * character.stats.jumpHorizontalSpeed, character.stats.jumpVelocity};
            setAction(fighter, Action::Airborne);
        }
        break;
    case Action::Landing:
        if (fighter.actionFrame >= character.stats.landingFrames) {
            setAction(fighter, Action::Idle);
        }
        break;
    case Action::Attack:
        // TODO(§4.5): эффекты кадра приёма (снаряд, рывок), по окончании totalFrames() → Idle.
        break;
    case Action::Hitstun:
    case Action::Blockstun:
        // TODO(§4.7): --stunFrames, по нулю → Idle (или Juggle/Knockdown).
        break;
    default:
        break;
    }
}

// --- Шаг 5: физика -----------------------------------------------------------------------------
void integrate(FighterState& fighter, const CharacterData& character, PlayerIndex player,
               EventList& events) noexcept {
    fighter.position.x += fighter.velocity.x;
    fighter.position.y += fighter.velocity.y;
    if (fighter.action == Action::Airborne) {
        fighter.velocity.y -= character.stats.gravity;
        if (fighter.position.y <= 0) {
            fighter.position.y = 0;
            fighter.velocity = {};
            setAction(fighter, Action::Landing);
            events.push(Landed{player});
        }
    }
}

// --- Шаг 6: пушбоксы, стены, ширина кадра ------------------------------------------------------
void resolvePushboxes(MatchState& state, const MatchData& data) noexcept {
    FighterState& left = state.fighters[0];
    FighterState& right = state.fighters[1];

    const Box a = toWorld(currentPushbox(left, data.characters[0]), left.position, left.facing);
    const Box b = toWorld(currentPushbox(right, data.characters[1]), right.position, right.facing);
    if (overlaps(a, b)) {
        const std::int32_t overlap = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
        const std::int32_t push = (overlap + 1) / 2;  // поровну обоим: порядок бойцов не важен
        if (left.position.x <= right.position.x) {
            left.position.x -= push;
            right.position.x += push;
        } else {
            left.position.x += push;
            right.position.x -= push;
        }
    }

    const std::int32_t distance = right.position.x - left.position.x;
    if (std::abs(distance) > data.rules.maxFighterDistance) {
        const std::int32_t center = (left.position.x + right.position.x) / 2;
        const std::int32_t half = data.rules.maxFighterDistance / 2;
        left.position.x = std::clamp(left.position.x, center - half, center + half);
        right.position.x = std::clamp(right.position.x, center - half, center + half);
    }

    // TODO(§4.3 шаг 6): углы — если один боец прижат к стене, расталкивание целиком достаётся другому.
    const std::int32_t wall = data.rules.arenaHalfWidth;
    for (FighterState& fighter : state.fighters) {
        fighter.position.x = std::clamp(fighter.position.x, -wall, wall);
    }
}

// --- Шаг 7: сторона взгляда --------------------------------------------------------------------
void updateFacing(MatchState& state) noexcept {
    for (std::size_t i = 0; i < state.fighters.size(); ++i) {
        FighterState& self = state.fighters[i];
        const FighterState& other = state.fighters[1 - i];
        if (isFreeToMove(self) && self.position.x != other.position.x) {
            self.facing = other.position.x > self.position.x ? 1 : -1;
        }
    }
}

// --- Шаги 8–10: снаряды и попадания ------------------------------------------------------------
void updateProjectiles(MatchState& /*state*/, const MatchData& /*data*/, EventList& /*events*/) noexcept {
    // TODO(§4.3 шаг 8): движение, столкновение снаряда со снарядом, выход за стены → ProjectileDestroyed.
}

void resolveHits(MatchState& /*state*/, const MatchData& /*data*/, EventList& /*events*/) noexcept {
    // TODO(§4.3 шаги 9–10): собрать ВСЕ пересечения hitbox × hurtbox обоих бойцов и снарядов,
    // затем применить одновременно (размен): урон со скейлингом, stun, отталкивание, hitstop, метр, HitEvent.
}

// --- Шаг 11: раунд -----------------------------------------------------------------------------
void updateRound(MatchState& state, const MatchData& data, EventList& events) noexcept {
    RoundState& round = state.round;
    ++round.phaseFrame;

    switch (round.phase) {
    case RoundPhase::Intro:
        if (round.phaseFrame == 1) {
            events.push(RoundStarted{round.number});
        }
        if (round.phaseFrame >= data.rules.introFrames) {
            enterPhase(round, RoundPhase::Fight);
            events.push(FightStarted{});
        }
        break;

    case RoundPhase::Fight:
        // TODO(§4.7): KO (health <= 0) → FinishHim в решающем раунде, иначе KnockOut и конец раунда.
        if (round.timerFrames > 0) {
            --round.timerFrames;
        }
        if (round.timerFrames == 0) {
            const PlayerIndex winner = winnerByHealth(state);
            ++state.fighter(winner).roundsWon;
            enterPhase(round, RoundPhase::Over);
            events.push(RoundEnded{winner, true, false});
        }
        break;

    case RoundPhase::FinishHim:
        // TODO(§4.7): ждём команду Fatality до finishHimFrames; любой другой удар — обычный KO.
        break;

    case RoundPhase::Over:
        if (round.phaseFrame == data.rules.roundOverFrames) {
            for (std::size_t i = 0; i < state.fighters.size(); ++i) {
                if (state.fighters[i].roundsWon >= data.rules.roundsToWin) {
                    events.push(MatchEnded{static_cast<PlayerIndex>(i)});
                    return;
                }
            }
            startRound(state, data, round.number + 1);
        }
        break;
    }
}

}  // namespace

MatchState makeInitialState(const MatchData& data, std::uint64_t seed) noexcept {
    MatchState state{};
    state.rng = core::Pcg32{seed};
    startRound(state, data, 1);
    return state;
}

void step(MatchState& state, const MatchData& data, const StepInput& input, EventList& events) noexcept {
    bufferInputs(state, input);  // 1

    if (state.hitstop > 0) {  // 2. TODO(§4.3): то же для кинематик (Action::Cinematic)
        --state.hitstop;
        ++state.frame;
        return;
    }

    const bool canAct = state.round.phase == RoundPhase::Fight;
    for (std::size_t i = 0; i < state.fighters.size(); ++i) {
        decideAction(state.fighters[i], data.characters[i], canAct);  // 3
    }
    for (std::size_t i = 0; i < state.fighters.size(); ++i) {
        advanceAction(state.fighters[i], data.characters[i]);  // 4
        integrate(state.fighters[i], data.characters[i], static_cast<PlayerIndex>(i), events);  // 5
    }
    resolvePushboxes(state, data);            // 6
    updateFacing(state);                      // 7
    updateProjectiles(state, data, events);   // 8
    resolveHits(state, data, events);         // 9–10
    updateRound(state, data, events);         // 11
    ++state.frame;                            // 12
}

}  // namespace dc::sim
