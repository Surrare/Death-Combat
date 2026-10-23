#include "game/input.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>

namespace dc {

void Input::attach(GLFWwindow* w) { window_ = w; }

void Input::setMouseCaptured(bool c) {
    captured_ = c;
    glfwSetInputMode(window_, GLFW_CURSOR, c ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    haveMouse_ = false;
}

static float deadzone(float v) {
    const float dz = 0.22f;
    float a = std::fabs(v);
    if (a < dz) return 0.0f;
    return (a - dz) / (1.0f - dz) * (v < 0 ? -1.0f : 1.0f);
}

void Input::poll() {
    auto key = [&](int k) { return glfwGetKey(window_, k) == GLFW_PRESS; };

    uint32_t held[2] = {0, 0};
    float ax[2] = {0, 0}, ay[2] = {0, 0}, lx[2] = {0, 0}, ly[2] = {0, 0};

    // ---- keyboard, player 1
    if (key(GLFW_KEY_W)) held[0] |= BTN_UP;
    if (key(GLFW_KEY_S)) held[0] |= BTN_DOWN;
    if (key(GLFW_KEY_A)) held[0] |= BTN_LEFT;
    if (key(GLFW_KEY_D)) held[0] |= BTN_RIGHT;
    if (key(GLFW_KEY_J)) held[0] |= BTN_LIGHT;
    if (key(GLFW_KEY_K)) held[0] |= BTN_MEDIUM;
    if (key(GLFW_KEY_L)) held[0] |= BTN_HEAVY;
    if (key(GLFW_KEY_I)) held[0] |= BTN_STYLE;
    if (key(GLFW_KEY_U) || key(GLFW_KEY_LEFT_SHIFT) || key(GLFW_KEY_SPACE)) held[0] |= BTN_DASH;
    if (key(GLFW_KEY_O)) held[0] |= BTN_TAUNT;
    if (key(GLFW_KEY_ENTER) || key(GLFW_KEY_E)) held[0] |= BTN_START;
    if (key(GLFW_KEY_ESCAPE)) held[0] |= BTN_BACK;
    if (key(GLFW_KEY_Q)) lx[0] -= 1.0f;
    if (key(GLFW_KEY_R)) lx[0] += 1.0f;

    // ---- keyboard, player 2 (arrows + numpad)
    if (key(GLFW_KEY_UP)) held[1] |= BTN_UP;
    if (key(GLFW_KEY_DOWN)) held[1] |= BTN_DOWN;
    if (key(GLFW_KEY_LEFT)) held[1] |= BTN_LEFT;
    if (key(GLFW_KEY_RIGHT)) held[1] |= BTN_RIGHT;
    if (key(GLFW_KEY_KP_1)) held[1] |= BTN_LIGHT;
    if (key(GLFW_KEY_KP_2)) held[1] |= BTN_MEDIUM;
    if (key(GLFW_KEY_KP_3)) held[1] |= BTN_HEAVY;
    if (key(GLFW_KEY_KP_4)) held[1] |= BTN_STYLE;
    if (key(GLFW_KEY_KP_5) || key(GLFW_KEY_KP_0)) held[1] |= BTN_DASH;
    if (key(GLFW_KEY_KP_6)) held[1] |= BTN_TAUNT;
    if (key(GLFW_KEY_KP_ENTER)) held[1] |= BTN_START;

    // ---- mouse look for player 1
    double mx, my;
    glfwGetCursorPos(window_, &mx, &my);
    if (captured_) {
        if (haveMouse_) {
            lx[0] += (float)(mx - lastMouseX_) * 0.12f;
            ly[0] += (float)(my - lastMouseY_) * 0.12f;
        }
        haveMouse_ = true;
    }
    lastMouseX_ = mx; lastMouseY_ = my;

    // ---- gamepads: joystick 0 -> player 1, joystick 1 -> player 2
    for (int p = 0; p < 2; ++p) {
        int jid = GLFW_JOYSTICK_1 + p;
        GLFWgamepadstate gs;
        if (!glfwJoystickPresent(jid) || !glfwGetGamepadState(jid, &gs)) continue;
        auto b = [&](int id) { return gs.buttons[id] == GLFW_PRESS; };
        if (b(GLFW_GAMEPAD_BUTTON_X)) held[p] |= BTN_LIGHT;
        if (b(GLFW_GAMEPAD_BUTTON_Y)) held[p] |= BTN_MEDIUM;
        if (b(GLFW_GAMEPAD_BUTTON_B)) held[p] |= BTN_HEAVY;
        if (b(GLFW_GAMEPAD_BUTTON_A)) held[p] |= BTN_STYLE;
        if (b(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER)) held[p] |= BTN_DASH;
        if (b(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER)) held[p] |= BTN_TAUNT;
        if (b(GLFW_GAMEPAD_BUTTON_START)) held[p] |= BTN_START;
        if (b(GLFW_GAMEPAD_BUTTON_BACK)) held[p] |= BTN_BACK;
        if (b(GLFW_GAMEPAD_BUTTON_DPAD_UP)) held[p] |= BTN_UP;
        if (b(GLFW_GAMEPAD_BUTTON_DPAD_DOWN)) held[p] |= BTN_DOWN;
        if (b(GLFW_GAMEPAD_BUTTON_DPAD_LEFT)) held[p] |= BTN_LEFT;
        if (b(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT)) held[p] |= BTN_RIGHT;
        float sx = deadzone(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
        float sy = -deadzone(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
        ax[p] += sx; ay[p] += sy;
        if (sx < -0.5f) held[p] |= BTN_LEFT;
        if (sx > 0.5f) held[p] |= BTN_RIGHT;
        if (sy > 0.5f) held[p] |= BTN_UP;
        if (sy < -0.5f) held[p] |= BTN_DOWN;
        lx[p] += deadzone(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]) * 3.0f;
        ly[p] += deadzone(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]) * 3.0f;
        if (gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f) held[p] |= BTN_DASH;
    }

    for (int p = 0; p < 2; ++p) {
        PadState& s = pads_[p];
        s.held = held[p];
        s.pressed = held[p] & ~prevHeld_[p];
        s.released = ~held[p] & prevHeld_[p];
        prevHeld_[p] = held[p];
        // SOCD: opposite directions cancel
        int dx = (s.down(BTN_RIGHT) ? 1 : 0) - (s.down(BTN_LEFT) ? 1 : 0);
        int dy = (s.down(BTN_UP) ? 1 : 0) - (s.down(BTN_DOWN) ? 1 : 0);
        s.dirX = dx; s.dirY = dy;
        float kx = (float)dx, ky = (float)dy;
        s.axisX = std::clamp(ax[p] + kx, -1.0f, 1.0f);
        s.axisY = std::clamp(ay[p] + ky, -1.0f, 1.0f);
        s.lookX = lx[p]; s.lookY = ly[p];
    }
}

} // namespace dc
