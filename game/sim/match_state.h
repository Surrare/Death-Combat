#pragma once

#include "core/pcg32.h"
#include "core/static_vector.h"
#include "sim/input_frame.h"
#include "sim/types.h"

#include <array>
#include <cstdint>
#include <type_traits>

namespace dc::sim {

struct FighterState {
    Vec2i position;
    Vec2i velocity;          // мм за тик
    std::int32_t facing{1};  // +1 смотрит вправо, -1 влево
    Action action{Action::Idle};
    MoveId move{MoveId::None};
    std::int32_t actionFrame{0};  // номер тика внутри текущего действия
    std::int32_t stunFrames{0};   // сколько ещё длится hit- или blockstun
    std::int32_t health{0};
    std::int32_t offenseMeter{0};  // 2 сегмента по 1000 (§4.7)
    std::int32_t defenseMeter{0};
    std::int32_t comboHits{0};  // удары подряд, которые этот боец провёл сопернику
    std::int32_t comboDamage{0};
    std::int32_t jugglePoints{0};
    std::int32_t roundsWon{0};
    bool moveConnected{false};  // текущий приём уже попал (в прототипе hasHitCurrentAttack)
    bool fatalBlowUsed{false};
    InputBuffer input;
};

struct Projectile {
    PlayerIndex owner{PlayerIndex::P1};
    Vec2i position;
    Vec2i velocity;
    Box hitbox;  // относительно position
    std::int32_t facing{1};
    std::int32_t damage{0};
};

enum class RoundPhase : std::uint8_t { Intro, Fight, FinishHim, Over };

struct RoundState {
    std::int32_t number{1};
    RoundPhase phase{RoundPhase::Intro};
    std::int32_t phaseFrame{0};   // тиков с начала фазы
    std::int32_t timerFrames{0};  // сколько тиков осталось до конца раунда
};

// Всё состояние боя. Изменяет его только sim::step (docs/ARCHITECTURE.md §4).
struct MatchState {
    std::uint32_t frame{0};
    std::array<FighterState, 2> fighters{};
    core::StaticVector<Projectile, 8> projectiles;
    RoundState round;
    std::int32_t hitstop{0};  // тиков глобальной заморозки после удара
    core::Pcg32 rng;

    [[nodiscard]] FighterState& fighter(PlayerIndex player) noexcept { return fighters[toIndex(player)]; }
    [[nodiscard]] const FighterState& fighter(PlayerIndex player) const noexcept { return fighters[toIndex(player)]; }
};

// Снимок = memcpy: тренировка, повторы, rollback (ADR-0003, ADR-0006).
// Добавляя поле, не кладите сюда std::vector, std::string и указатели.
static_assert(std::is_trivially_copyable_v<MatchState>, "MatchState обязан копироваться через memcpy");

}  // namespace dc::sim
