#pragma once

#include "sim/input_frame.h"
#include "sim/match_data.h"
#include "sim/types.h"

#include <cstdint>

namespace dc::sim {

// Направление относительно экрана → относительно бойца: при facing = -1 «вперёд» — это влево.
[[nodiscard]] constexpr std::uint8_t toRelativeDirection(std::uint8_t screenDirection, std::int32_t facing) noexcept {
    if (facing >= 0) {
        return screenDirection;
    }
    const int row = (screenDirection - 1) / 3;
    const int column = (screenDirection - 1) % 3;
    return static_cast<std::uint8_t>(row * 3 + (2 - column) + 1);  // 4↔6, 1↔3, 7↔9
}

// Приём с наивысшим приоритетом, введённый в буфере: Fatality > Fatal Blow > спецприём > строка > удар.
// TODO(§4.3 шаг 3, §5): разбор нотации "B,F+2", окна снисходительности, приоритеты.
[[nodiscard]] MoveId findCommand(const InputBuffer& buffer, const CharacterData& character,
                                 std::int32_t facing) noexcept;

}  // namespace dc::sim
