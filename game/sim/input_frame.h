#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dc::sim {

// Кнопки в нотации MK11 (docs/ARCHITECTURE.md §5).
enum class Button : std::uint8_t {
    FrontPunch = 1 << 0,  // «1»
    BackPunch = 1 << 1,   // «2»
    FrontKick = 1 << 2,   // «3»
    BackKick = 1 << 3,    // «4»
    Block = 1 << 4,
    Throw = 1 << 5,
};

// Направления — нумпад-нотация (7 8 9 / 4 5 6 / 1 2 3), 5 — нейтраль.
// В InputFrame они относительно ЭКРАНА; «вперёд» и «назад» относительно бойца считает CommandInterpreter.
[[nodiscard]] constexpr std::uint8_t makeDirection(int horizontal, int vertical) noexcept {
    return static_cast<std::uint8_t>(5 + horizontal + 3 * vertical);  // horizontal: -1 влево, vertical: +1 вверх
}
[[nodiscard]] constexpr int horizontalOf(std::uint8_t direction) noexcept {
    return (direction - 1) % 3 - 1;
}
[[nodiscard]] constexpr bool isUp(std::uint8_t direction) noexcept {
    return direction >= 7;
}
[[nodiscard]] constexpr bool isDown(std::uint8_t direction) noexcept {
    return direction <= 3;
}

// Ввод одного игрока за один тик. Его выдают и клавиатура, и геймпад, и ИИ (ADR-0009).
struct InputFrame {
    std::uint8_t direction{5};
    std::uint8_t buttons{0};  // маска зажатых Button; «нажатия» симуляция вычисляет по буферу

    [[nodiscard]] constexpr bool held(Button button) const noexcept {
        return (buttons & static_cast<std::uint8_t>(button)) != 0;
    }

    friend constexpr bool operator==(const InputFrame&, const InputFrame&) = default;
};

// Последние 32 кадра ввода бойца. Лежит внутри FighterState, поэтому тоже trivially copyable.
class InputBuffer {
public:
    static constexpr int capacity = 32;

    constexpr void push(InputFrame frame) noexcept {
        head_ = (head_ + 1) % capacity;
        frames_[static_cast<std::size_t>(head_)] = frame;
    }

    // age = 0 — текущий тик, 1 — предыдущий и т. д.
    [[nodiscard]] constexpr InputFrame at(int age) const noexcept {
        const int index = ((head_ - age) % capacity + capacity) % capacity;
        return frames_[static_cast<std::size_t>(index)];
    }

    // Кнопка нажата (в кадре раньше не была зажата) не позже maxAge тиков назад — окно буфера (§5).
    [[nodiscard]] constexpr bool pressedWithin(Button button, int maxAge) const noexcept {
        for (int age = 0; age <= maxAge && age + 1 < capacity; ++age) {
            if (at(age).held(button) && !at(age + 1).held(button)) {
                return true;
            }
        }
        return false;
    }

private:
    std::array<InputFrame, capacity> frames_{};
    int head_{0};
};

}  // namespace dc::sim
