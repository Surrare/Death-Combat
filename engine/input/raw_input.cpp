#include "input/raw_input.h"

#include <GLFW/glfw3.h>

namespace dc::input {
namespace {

bool inRange(int key) noexcept {
    return key >= 0 && static_cast<std::size_t>(key) < maxKeys;
}

}  // namespace

bool RawInputState::isDown(int key) const noexcept {
    return inRange(key) && keys.test(static_cast<std::size_t>(key));
}

bool RawInputState::wasPressed(int key) const noexcept {
    return isDown(key) && !previousKeys.test(static_cast<std::size_t>(key));
}

void InputDevices::poll(GLFWwindow* window) {
    state_.previousKeys = state_.keys;
    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
        state_.keys.set(static_cast<std::size_t>(key), glfwGetKey(window, key) == GLFW_PRESS);
    }

    for (std::size_t i = 0; i < maxGamepads; ++i) {
        GamepadState& pad = state_.gamepads[i];
        const int joystick = GLFW_JOYSTICK_1 + static_cast<int>(i);
        GLFWgamepadstate glfwState{};
        pad.connected = glfwJoystickIsGamepad(joystick) == GLFW_TRUE &&
                        glfwGetGamepadState(joystick, &glfwState) == GLFW_TRUE;
        if (!pad.connected) {
            continue;
        }
        for (std::size_t b = 0; b < pad.buttons.size(); ++b) {
            pad.buttons[b] = glfwState.buttons[b] == GLFW_PRESS;
        }
        for (std::size_t a = 0; a < pad.axes.size(); ++a) {
            pad.axes[a] = glfwState.axes[a];
        }
    }
}

}  // namespace dc::input
