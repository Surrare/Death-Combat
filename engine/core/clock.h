#pragma once

#include <chrono>

namespace dc::core {

// Монотонные часы в секундах. Только для главного цикла и представления:
// симуляция живёт кадрами и часов не знает (docs/ARCHITECTURE.md §3).
class Clock {
public:
    Clock() noexcept : start_{std::chrono::steady_clock::now()} {}

    [[nodiscard]] double now() const noexcept {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_;
};

}  // namespace dc::core
