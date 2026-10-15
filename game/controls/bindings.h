#pragma once

#include "input/raw_input.h"
#include "sim/input_frame.h"
#include "sim/types.h"

#include <cstdint>

namespace dc::controls {

// Раскладка клавиатуры одного игрока: коды GLFW_KEY_* (docs/ARCHITECTURE.md §5).
struct KeyboardLayout {
    int up{0};
    int down{0};
    int left{0};
    int right{0};
    int frontPunch{0};
    int backPunch{0};
    int frontKick{0};
    int backKick{0};
    int block{0};
    int throwButton{0};
};

[[nodiscard]] KeyboardLayout defaultKeyboardLayout(sim::PlayerIndex player) noexcept;

// SOCD-очистка: лево + право = нейтраль, верх + низ = нейтраль. Результат — нумпад относительно экрана.
[[nodiscard]] std::uint8_t directionFromKeys(bool up, bool down, bool left, bool right) noexcept;

[[nodiscard]] sim::InputFrame readKeyboard(const input::RawInputState& raw, const KeyboardLayout& layout) noexcept;
[[nodiscard]] sim::InputFrame readGamepad(const input::GamepadState& pad) noexcept;

// Ввод игрока за тик: клавиатура + геймпад с тем же номером (P1 — первый, P2 — второй).
// TODO(§5): раскладки из %APPDATA%/DeathCombat/settings.json и меню переназначения.
[[nodiscard]] sim::InputFrame readPlayer(const input::RawInputState& raw, sim::PlayerIndex player) noexcept;

}  // namespace dc::controls
