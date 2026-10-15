#pragma once

#include <cstdint>

namespace dc::core {

// Ссылка на ресурс без владения: индекс слота + поколение (docs/ARCHITECTURE.md §9).
// Когда слот переиспользуют, поколение растёт, и старый хэндл перестаёт совпадать —
// вместо висячего указателя получаем понятную ошибку.
template <typename T>
struct Handle {
    static constexpr std::uint32_t invalidIndex = 0xFFFF'FFFF;

    std::uint32_t index{invalidIndex};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool isValid() const noexcept { return index != invalidIndex; }

    friend constexpr bool operator==(const Handle&, const Handle&) = default;
};

}  // namespace dc::core
