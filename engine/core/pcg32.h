#pragma once

#include <cstdint>

namespace dc::core {

// Генератор случайных чисел PCG32 (O'Neill). Детерминированный и копируемый:
// его состояние входит в MatchState, поэтому повторы и тесты воспроизводимы (docs/ARCHITECTURE.md §4.1).
// std::rand() в проекте не используем.
class Pcg32 {
public:
    constexpr Pcg32() noexcept = default;

    constexpr explicit Pcg32(std::uint64_t seed, std::uint64_t stream = 0xda3e39cb94b95bdbULL) noexcept {
        reseed(seed, stream);
    }

    constexpr void reseed(std::uint64_t seed, std::uint64_t stream) noexcept {
        state_ = 0;
        increment_ = (stream << 1u) | 1u;
        next();
        state_ += seed;
        next();
    }

    constexpr std::uint32_t next() noexcept {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + increment_;
        const auto xorShifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rotation = static_cast<std::uint32_t>(old >> 59u);
        return (xorShifted >> rotation) | (xorShifted << ((~rotation + 1u) & 31u));
    }

    // Равномерно в [0, bound), bound > 0. Отбраковка убирает перекос остатка от деления.
    constexpr std::uint32_t nextBelow(std::uint32_t bound) noexcept {
        const std::uint32_t threshold = (~bound + 1u) % bound;
        for (;;) {
            const std::uint32_t value = next();
            if (value >= threshold) {
                return value % bound;
            }
        }
    }

    friend constexpr bool operator==(const Pcg32&, const Pcg32&) = default;

private:
    std::uint64_t state_{0x853c49e6748fea9bULL};
    std::uint64_t increment_{0xda3e39cb94b95bdbULL};
};

}  // namespace dc::core
