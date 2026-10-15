#pragma once

#include "core/assert.h"

#include <array>
#include <cstddef>
#include <type_traits>

namespace dc::core {

// Вектор фиксированной ёмкости без кучи. Если T trivially copyable, то и StaticVector тоже —
// поэтому он годится для MatchState: снимок делается одним memcpy (docs/ARCHITECTURE.md §4.1).
template <typename T, std::size_t Capacity>
class StaticVector {
public:
    static_assert(std::is_default_constructible_v<T>);

    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr bool full() const noexcept { return size_ == Capacity; }

    // false, если места нет: в симуляции переполнение — повод проигнорировать, а не упасть.
    constexpr bool push(const T& value) noexcept {
        if (full()) {
            return false;
        }
        items_[size_++] = value;
        return true;
    }

    // Удаление за O(1): на место удалённого встаёт последний, порядок не сохраняется.
    constexpr void removeAt(std::size_t index) noexcept {
        DC_ASSERT(index < size_);
        items_[index] = items_[size_ - 1];
        --size_;
    }

    constexpr void clear() noexcept { size_ = 0; }

    [[nodiscard]] constexpr T& operator[](std::size_t index) noexcept {
        DC_ASSERT(index < size_);
        return items_[index];
    }
    [[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept {
        DC_ASSERT(index < size_);
        return items_[index];
    }

    [[nodiscard]] constexpr T* begin() noexcept { return items_.data(); }
    [[nodiscard]] constexpr T* end() noexcept { return items_.data() + size_; }
    [[nodiscard]] constexpr const T* begin() const noexcept { return items_.data(); }
    [[nodiscard]] constexpr const T* end() const noexcept { return items_.data() + size_; }

private:
    std::array<T, Capacity> items_{};
    std::size_t size_{0};
};

}  // namespace dc::core
