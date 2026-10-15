#pragma once

#include "core/static_vector.h"
#include "sim/types.h"

#include <cstdint>
#include <variant>

namespace dc::sim {

// События тика. Симуляция ничего не рисует и не озвучивает — она сообщает, что произошло,
// а представление (VFX, звук, камера, HUD) решает, как это показать (docs/ARCHITECTURE.md §4.8).

struct RoundStarted {
    std::int32_t round{1};
};

struct FightStarted {};

struct MoveStarted {
    PlayerIndex player{PlayerIndex::P1};
    MoveId move{MoveId::None};
};

struct HitEvent {
    PlayerIndex attacker{PlayerIndex::P1};
    Vec2i contactPoint;
    AttackHeight height{AttackHeight::High};
    HitStrength strength{HitStrength::Light};
    std::int32_t damage{0};
    bool blocked{false};
    bool counterHit{false};
};

struct ThrowEvent {
    PlayerIndex attacker{PlayerIndex::P1};
    bool teched{false};
};

struct ProjectileSpawned {
    PlayerIndex owner{PlayerIndex::P1};
    Vec2i position;
};

struct ProjectileDestroyed {
    Vec2i position;
};

struct Landed {
    PlayerIndex player{PlayerIndex::P1};
};

struct KnockedDown {
    PlayerIndex player{PlayerIndex::P1};
};

struct KnockOut {
    PlayerIndex loser{PlayerIndex::P1};
};

struct FinishHim {
    PlayerIndex loser{PlayerIndex::P1};
};

struct FatalityStarted {
    PlayerIndex winner{PlayerIndex::P1};
    std::int32_t fatality{0};  // индекс Fatality в данных бойца
};

struct FatalBlowStarted {
    PlayerIndex attacker{PlayerIndex::P1};
};

struct RoundEnded {
    PlayerIndex winner{PlayerIndex::P1};
    bool timeout{false};
    bool flawless{false};
};

struct MatchEnded {
    PlayerIndex winner{PlayerIndex::P1};
};

using GameEvent = std::variant<RoundStarted, FightStarted, MoveStarted, HitEvent, ThrowEvent, ProjectileSpawned,
                               ProjectileDestroyed, Landed, KnockedDown, KnockOut, FinishHim, FatalityStarted,
                               FatalBlowStarted, RoundEnded, MatchEnded>;

using EventList = core::StaticVector<GameEvent, 64>;

}  // namespace dc::sim
