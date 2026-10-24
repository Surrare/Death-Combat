#include "game/input.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <algorithm>

namespace dc {

void Input::attach(GLFWwindow* w, const Settings* s) { window_ = w; settings_ = s; }

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
    auto bound = [&](Action a) {
        for (int k : settings_->keys[(int)a]) if (k && key(k)) return true;
        return false;
    };
    if (bound(Action::Up)) held[0] |= BTN_UP;
    if (bound(Action::Down)) held[0] |= BTN_DOWN;
    if (bound(Action::Left)) held[0] |= BTN_LEFT;
    if (bound(Action::Right)) held[0] |= BTN_RIGHT;
    if (bound(Action::Light)) held[0] |= BTN_LIGHT;
    if (bound(Action::Medium)) held[0] |= BTN_MEDIUM;
    if (bound(Action::Heavy)) held[0] |= BTN_HEAVY;
    if (bound(Action::Style)) held[0] |= BTN_STYLE;
    if (bound(Action::Dash)) held[0] |= BTN_DASH;
    if (bound(Action::Taunt)) held[0] |= BTN_TAUNT;
    if (key(GLFW_KEY_ENTER)) held[0] |= BTN_START;
    if (key(GLFW_KEY_ESCAPE)) held[0] |= BTN_BACK;
    if (bound(Action::LookLeft)) lx[0] -= 1.0f;
    if (bound(Action::LookRight)) lx[0] += 1.0f;

    // first newly pressed key, used by the rebinding screen
    newKey_ = 0;
    for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST && k < 400; ++k) {
        bool d = key(k);
        if (d && !prevKeys_[k] && !newKey_) newKey_ = k;
        prevKeys_[k] = d;
    }

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
            lx[0] += (float)(mx - lastMouseX_) * settings_->mouseScale();
            ly[0] += (float)(my - lastMouseY_) * settings_->mouseScale();
        }
        haveMouse_ = true;
    }
    mouseMoved_ = mx != lastMouseX_ || my != lastMouseY_;
    lastMouseX_ = mx; lastMouseY_ = my;
    {
        int ww, wh, fw, fh;
        glfwGetWindowSize(window_, &ww, &wh);
        glfwGetFramebufferSize(window_, &fw, &fh);
        float sx = ww > 0 ? (float)fw / (float)ww : 1.0f, sy = wh > 0 ? (float)fh / (float)wh : 1.0f;
        mouseX_ = (float)mx * sx; mouseY_ = (float)my * sy;
        bool l = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool r = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        leftClicked_ = l && !prevLeft_; rightClicked_ = r && !prevRight_;
        prevLeft_ = l; prevRight_ = r;
    }

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

    // merged pad for menus and hunt movement: either player's keys work
    PadState& m = menu_;
    m.held = pads_[0].held | pads_[1].held;
    m.pressed = m.held & ~prevMenuHeld_;
    m.released = ~m.held & prevMenuHeld_;
    prevMenuHeld_ = m.held;
    int mdx = (m.down(BTN_RIGHT) ? 1 : 0) - (m.down(BTN_LEFT) ? 1 : 0);
    int mdy = (m.down(BTN_UP) ? 1 : 0) - (m.down(BTN_DOWN) ? 1 : 0);
    m.dirX = mdx; m.dirY = mdy;
    m.axisX = std::clamp(pads_[0].axisX + pads_[1].axisX, -1.0f, 1.0f);
    m.axisY = std::clamp(pads_[0].axisY + pads_[1].axisY, -1.0f, 1.0f);
    m.lookX = pads_[0].lookX + pads_[1].lookX;
    m.lookY = pads_[0].lookY + pads_[1].lookY;
}

} // namespace dc
