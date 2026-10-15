#pragma once

#include <array>
#include <bitset>
#include <cstddef>

struct GLFWwindow;

namespace dc::input {

inline constexpr std::size_t maxKeys = 512;  // все коды GLFW_KEY_* меньше 512
inline constexpr std::size_t maxGamepads = 4;

struct GamepadState {
    bool connected{false};
    std::array<bool, 15> buttons{};  // индексы GLFW_GAMEPAD_BUTTON_*
    std::array<float, 6> axes{};     // индексы GLFW_GAMEPAD_AXIS_*
};

// Сырые состояния устройств за кадр. О «ударе» и «блоке» здесь ничего не знают:
// перевод клавиш в действия бойца — забота game/controls (docs/ARCHITECTURE.md §5).
struct RawInputState {
    std::bitset<maxKeys> keys;
    std::bitset<maxKeys> previousKeys;
    std::array<GamepadState, maxGamepads> gamepads{};

    [[nodiscard]] bool isDown(int key) const noexcept;
    [[nodiscard]] bool wasPressed(int key) const noexcept;  // нажата именно в этом кадре
};

class InputDevices {
public:
    void poll(GLFWwindow* window);  // раз за кадр, после glfwPollEvents
    [[nodiscard]] const RawInputState& state() const noexcept { return state_; }

private:
    RawInputState state_;
};

}  // namespace dc::input
