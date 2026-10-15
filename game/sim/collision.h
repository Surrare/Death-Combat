#pragma once

#include "sim/types.h"

#include <cstdint>

namespace dc::sim {

// Локальный бокс (+x — по взгляду) → мировой: отражаем по facing и сдвигаем в позицию бойца (§4.6).
[[nodiscard]] constexpr Box toWorld(const Box& local, const Vec2i& position, std::int32_t facing) noexcept {
    if (facing >= 0) {
        return Box{{position.x + local.min.x, position.y + local.min.y},
                   {position.x + local.max.x, position.y + local.max.y}};
    }
    return Box{{position.x - local.max.x, position.y + local.min.y},
               {position.x - local.min.x, position.y + local.max.y}};
}

[[nodiscard]] constexpr bool overlaps(const Box& a, const Box& b) noexcept {
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

}  // namespace dc::sim
