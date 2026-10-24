// Keyboard + gamepad input, mapped to fighting-game buttons (JoJo ASB layout).
#pragma once
#include <cstdint>
#include "game/settings.h"
struct GLFWwindow;

namespace dc {

enum Button : uint32_t {
    BTN_LIGHT = 1u << 0,   // Square  / X     / J
    BTN_MEDIUM = 1u << 1,  // Triangle/ Y     / K
    BTN_HEAVY = 1u << 2,   // Circle  / B     / L
    BTN_STYLE = 1u << 3,   // Cross   / A     / I   (Stand on/off)
    BTN_DASH = 1u << 4,    // R1      / RB    / U   (dash / stylish evade)
    BTN_TAUNT = 1u << 5,   // L1      / LB    / O
    BTN_START = 1u << 6,   // Start   / Enter
    BTN_BACK = 1u << 7,    // Select  / Escape
    BTN_UP = 1u << 8,
    BTN_DOWN = 1u << 9,
    BTN_LEFT = 1u << 10,
    BTN_RIGHT = 1u << 11,
};

struct PadState {
    float axisX = 0, axisY = 0;      // analog movement, -1..1 (y: +1 = forward/up)
    float lookX = 0, lookY = 0;      // camera look (right stick / mouse delta scaled)
    int dirX = 0, dirY = 0;          // digital direction, -1..1 (SOCD cleaned)
    uint32_t held = 0, pressed = 0, released = 0;
    bool down(uint32_t b) const { return (held & b) != 0; }
    bool justPressed(uint32_t b) const { return (pressed & b) != 0; }
};

class Input {
public:
    void attach(GLFWwindow* window, const Settings* settings);
    void poll();                      // call once per frame
    const PadState& player(int i) const { return pads_[i]; }
    // Both players' controls merged: arrows + WASD, Enter / J confirm, Esc back. Use for menus.
    const PadState& menu() const { return menu_; }
    bool anyPressed() const { return (pads_[0].pressed | pads_[1].pressed) != 0; }
    void setMouseCaptured(bool captured);
    bool mouseCaptured() const { return captured_; }
    int newKey() const { return newKey_; }   // key pressed this frame (for rebinding), 0 if none

private:
    GLFWwindow* window_ = nullptr;
    PadState pads_[2];
    PadState menu_;
    uint32_t prevMenuHeld_ = 0;
    uint32_t prevHeld_[2] = {0, 0};
    double lastMouseX_ = 0, lastMouseY_ = 0;
    bool haveMouse_ = false;
    bool captured_ = false;
    const Settings* settings_ = nullptr;
    bool prevKeys_[400] = {};
    int newKey_ = 0;
};

} // namespace dc
