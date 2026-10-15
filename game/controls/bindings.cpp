#include "controls/bindings.h"

#include <GLFW/glfw3.h>

namespace dc::controls {
namespace {

constexpr float stickDeadZone = 0.5f;
constexpr float triggerThreshold = 0.5f;

std::uint8_t buttonBit(sim::Button button) noexcept {
    return static_cast<std::uint8_t>(button);
}

}  // namespace

KeyboardLayout defaultKeyboardLayout(sim::PlayerIndex player) noexcept {
    if (player == sim::PlayerIndex::P1) {
        return KeyboardLayout{GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_U,
                              GLFW_KEY_I, GLFW_KEY_J, GLFW_KEY_K, GLFW_KEY_L, GLFW_KEY_O};
    }
    return KeyboardLayout{GLFW_KEY_UP,   GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_KP_4,
                          GLFW_KEY_KP_5, GLFW_KEY_KP_1, GLFW_KEY_KP_2, GLFW_KEY_KP_0,  GLFW_KEY_KP_6};
}

std::uint8_t directionFromKeys(bool up, bool down, bool left, bool right) noexcept {
    const int horizontal = (right ? 1 : 0) - (left ? 1 : 0);
    const int vertical = (up ? 1 : 0) - (down ? 1 : 0);
    return sim::makeDirection(horizontal, vertical);
}

sim::InputFrame readKeyboard(const input::RawInputState& raw, const KeyboardLayout& layout) noexcept {
    sim::InputFrame frame;
    frame.direction = directionFromKeys(raw.isDown(layout.up), raw.isDown(layout.down), raw.isDown(layout.left),
                                        raw.isDown(layout.right));
    const auto set = [&](int key, sim::Button button) {
        if (raw.isDown(key)) {
            frame.buttons = static_cast<std::uint8_t>(frame.buttons | buttonBit(button));
        }
    };
    set(layout.frontPunch, sim::Button::FrontPunch);
    set(layout.backPunch, sim::Button::BackPunch);
    set(layout.frontKick, sim::Button::FrontKick);
    set(layout.backKick, sim::Button::BackKick);
    set(layout.block, sim::Button::Block);
    set(layout.throwButton, sim::Button::Throw);
    return frame;
}

sim::InputFrame readGamepad(const input::GamepadState& pad) noexcept {
    sim::InputFrame frame;
    if (!pad.connected) {
        return frame;
    }
    const float stickX = pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    const float stickY = pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];  // вниз — положительный
    const bool up = pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] || stickY < -stickDeadZone;
    const bool down = pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] || stickY > stickDeadZone;
    const bool left = pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] || stickX < -stickDeadZone;
    const bool right = pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] || stickX > stickDeadZone;
    frame.direction = directionFromKeys(up, down, left, right);

    const auto set = [&](bool pressed, sim::Button button) {
        if (pressed) {
            frame.buttons = static_cast<std::uint8_t>(frame.buttons | buttonBit(button));
        }
    };
    set(pad.buttons[GLFW_GAMEPAD_BUTTON_X], sim::Button::FrontPunch);
    set(pad.buttons[GLFW_GAMEPAD_BUTTON_Y], sim::Button::BackPunch);
    set(pad.buttons[GLFW_GAMEPAD_BUTTON_A], sim::Button::FrontKick);
    set(pad.buttons[GLFW_GAMEPAD_BUTTON_B], sim::Button::BackKick);
    set(pad.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > triggerThreshold, sim::Button::Block);
    set(pad.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER], sim::Button::Throw);
    return frame;
}

sim::InputFrame readPlayer(const input::RawInputState& raw, sim::PlayerIndex player) noexcept {
    const sim::InputFrame keyboard = readKeyboard(raw, defaultKeyboardLayout(player));
    const sim::InputFrame gamepad = readGamepad(raw.gamepads[sim::toIndex(player)]);

    sim::InputFrame merged;
    merged.direction = keyboard.direction != 5 ? keyboard.direction : gamepad.direction;
    merged.buttons = static_cast<std::uint8_t>(keyboard.buttons | gamepad.buttons);
    return merged;
}

}  // namespace dc::controls
