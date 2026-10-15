#pragma once

#include <cstddef>
#include <cstdint>

namespace dc::sim {

inline constexpr std::int32_t ticksPerSecond = 60;

// Все координаты симуляции — целые миллиметры: детерминизм без float (ADR-0003).
// Скорости — мм за тик, ускорения — мм за тик². В метры их переводит только представление.
struct Vec2i {
    std::int32_t x{0};
    std::int32_t y{0};

    friend constexpr bool operator==(const Vec2i&, const Vec2i&) = default;
};

// AABB. В данных бойца задаётся в локальных координатах: +x — направление взгляда, y = 0 — пол.
struct Box {
    Vec2i min;
    Vec2i max;

    friend constexpr bool operator==(const Box&, const Box&) = default;
};

enum class PlayerIndex : std::uint8_t { P1 = 0, P2 = 1 };

[[nodiscard]] constexpr std::size_t toIndex(PlayerIndex player) noexcept {
    return static_cast<std::size_t>(player);
}

[[nodiscard]] constexpr PlayerIndex opponentOf(PlayerIndex player) noexcept {
    return player == PlayerIndex::P1 ? PlayerIndex::P2 : PlayerIndex::P1;
}

// Действие бойца (docs/ARCHITECTURE.md §4.4). Состояние — enum + данные, а не классы с virtual:
// так MatchState остаётся trivially copyable (ADR-0006).
enum class Action : std::uint8_t {
    Idle,
    WalkForward,
    WalkBack,
    Crouch,
    JumpSquat,
    Airborne,
    Landing,
    Attack,
    Blockstun,
    Hitstun,
    Juggle,
    Knockdown,
    Wakeup,
    Throwing,
    Thrown,
    Dizzy,
    Cinematic,
    Victory,
    Defeated,
};

enum class MoveId : std::uint16_t { None = 0xFFFF };  // иначе — индекс в CharacterData::moves

enum class AttackHeight : std::uint8_t { High, Mid, Low, Overhead };

enum class HitStrength : std::uint8_t { Light, Medium, Heavy, Special };

}  // namespace dc::sim
