// Settings screen: audio levels, video options, mouse sensitivity and key rebinding.
#include "game/app.h"
#include <GLFW/glfw3.h>
#include <algorithm>

namespace dc {

namespace {
const Color WHITE(1, 1, 1), GOLD(1.0f, 0.82f, 0.3f), RED(0.95f, 0.2f, 0.2f), DIM(0.55f, 0.55f, 0.6f), CYAN(0.4f, 0.9f, 1.0f);

enum Row { ROW_MASTER, ROW_MUSIC, ROW_SFX, ROW_FULLSCREEN, ROW_VSYNC, ROW_MOUSE, ROW_KEYS, ROW_RESET = ROW_KEYS + ACTION_COUNT, ROW_BACK, ROW_COUNT };
}

// shared by update (mouse hit-testing) and render so the two never disagree
static float settingsRowH(int height) { return std::min(26.0f, (height - 150.0f) / ROW_COUNT); }
static float settingsRowY(int r, float rowH) { return 80.0f + r * rowH + (r >= ROW_KEYS ? 22.0f : 0.0f); }
static const float CHAR_W = 6.0f * 1.7f;   // text scale 1.7 used for settings rows

void App::openSettings(Scene returnTo) {
    settingsReturn_ = returnTo;
    settingsIndex_ = 0; settingsSlot_ = 0; settingsCapturing_ = false;
    scene_ = Scene::Settings;
}

void App::applyVideoSettings() {
    glfwSwapInterval(settings_.vsync ? 1 : 0);
    bool isFull = glfwGetWindowMonitor(window_) != nullptr;
    if (settings_.fullscreen == isFull) return;
    if (settings_.fullscreen) {
        glfwGetWindowPos(window_, &windowedX_, &windowedY_);
        glfwGetWindowSize(window_, &windowedW_, &windowedH_);
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* vm = mon ? glfwGetVideoMode(mon) : nullptr;
        if (vm) glfwSetWindowMonitor(window_, mon, 0, 0, vm->width, vm->height, vm->refreshRate);
    } else {
        glfwSetWindowMonitor(window_, nullptr, windowedX_, windowedY_, windowedW_, windowedH_, 0);
    }
}

void App::closeSettings() {
    settings_.save();
    scene_ = settingsReturn_;
}

void App::updateSettings(float dt) {
    if (input_.mouseCaptured()) input_.setMouseCaptured(false);
    const PadState& m = input_.menu();

    if (settingsCapturing_) {
        int k = input_.newKey();
        if (k == GLFW_KEY_ESCAPE || input_.rightClicked()) settingsCapturing_ = false;
        else if (k) {
            settings_.bind((Action)(settingsIndex_ - ROW_KEYS), settingsSlot_, k);
            settingsCapturing_ = false;
        }
        return;
    }

    // mouse: hover selects, left click acts, right click goes back
    bool clicked = false;
    int clickDx = 0;
    {
        float cx = width_ * 0.5f, rowH = settingsRowH(height_), labelX = cx - 300, valueX = cx + 20;
        for (int r = 0; r < ROW_COUNT; ++r) {
            float y = settingsRowY(r, rowH);
            if (!input_.mouseIn(labelX - 12, y - 4, 640, rowH - 2)) continue;
            if (input_.mouseMoved() || input_.leftClicked()) settingsIndex_ = r;
            if (!input_.leftClicked()) break;
            float mx = input_.mouseX();
            if (r >= ROW_KEYS && r < ROW_RESET) {
                if (mx >= valueX - 6) { settingsSlot_ = std::clamp((int)((mx - valueX + 6) / 190), 0, BIND_SLOTS - 1); clicked = true; }
            } else if (r == ROW_MASTER || r == ROW_MUSIC || r == ROW_SFX || r == ROW_MOUSE) {
                int& v = r == ROW_MASTER ? settings_.master : r == ROW_MUSIC ? settings_.music : r == ROW_SFX ? settings_.sfx : settings_.mouseSens;
                int lo = r == ROW_MOUSE ? 1 : 0;
                float barX = valueX + 3 * CHAR_W;
                if (mx >= barX && mx < barX + 10 * CHAR_W) v = std::clamp((int)((mx - barX) / CHAR_W) + 1, lo, 10);
                else if (mx >= valueX && mx < barX) clickDx = -1;
                else if (mx >= barX + 10 * CHAR_W && mx < barX + 10 * CHAR_W + 5 * CHAR_W) clickDx = 1;
            } else if (r == ROW_FULLSCREEN || r == ROW_VSYNC) {
                if (mx >= valueX) clicked = true;
            } else clicked = true;
            break;
        }
    }

    if (m.justPressed(BTN_DOWN)) settingsIndex_ = (settingsIndex_ + 1) % ROW_COUNT;
    if (m.justPressed(BTN_UP)) settingsIndex_ = (settingsIndex_ + ROW_COUNT - 1) % ROW_COUNT;
    int dx = (m.justPressed(BTN_RIGHT) ? 1 : 0) - (m.justPressed(BTN_LEFT) ? 1 : 0) + clickDx;
    bool confirm = m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT) || clicked;
    int r = settingsIndex_;

    auto slider = [&](int& v, int lo, int hi) {
        if (dx) v = std::clamp(v + dx, lo, hi);
        else if (confirm) v = v >= hi ? lo : v + 1;
    };
    if (r == ROW_MASTER) slider(settings_.master, 0, 10);
    else if (r == ROW_MUSIC) slider(settings_.music, 0, 10);
    else if (r == ROW_SFX) slider(settings_.sfx, 0, 10);
    else if (r == ROW_MOUSE) slider(settings_.mouseSens, 1, 10);
    else if (r == ROW_FULLSCREEN && (dx || confirm)) { settings_.fullscreen = !settings_.fullscreen; applyVideoSettings(); }
    else if (r == ROW_VSYNC && (dx || confirm)) { settings_.vsync = !settings_.vsync; applyVideoSettings(); }
    else if (r >= ROW_KEYS && r < ROW_RESET) {
        if (dx) settingsSlot_ = std::clamp(settingsSlot_ + dx, 0, BIND_SLOTS - 1);
        if (confirm) settingsCapturing_ = true;
        if (glfwGetKey(window_, GLFW_KEY_BACKSPACE) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_DELETE) == GLFW_PRESS)
            settings_.keys[r - ROW_KEYS][settingsSlot_] = 0;
    }
    else if (r == ROW_RESET && confirm) settings_.resetKeys();
    else if (r == ROW_BACK && confirm) { closeSettings(); return; }

    if (m.justPressed(BTN_BACK) || input_.rightClicked()) closeSettings();
}

void App::renderSettings() {
    renderer_.beginFrame(Camera(), Color(0.02f, 0.02f, 0.03f));
    float cx = width_ * 0.5f;
    renderer_.textCentered(cx, 24, "SETTINGS", 4, GOLD);

    const float rowH = settingsRowH(height_);
    const float labelX = cx - 300, valueX = cx + 20;
    auto bar = [&](float y, int v, int maxV) {
        std::string s = "<  ";
        for (int i = 0; i < maxV; ++i) s += i < v ? "#" : "-";
        return s + "  >";
    };
    for (int r = 0; r < ROW_COUNT; ++r) {
        float y = settingsRowY(r, rowH);
        if (r == ROW_KEYS) renderer_.text(labelX, y - 22, "CONTROLS   (KEYBOARD, PLAYER 1)", 1.6f, CYAN);
        bool sel = r == settingsIndex_;
        if (sel) renderer_.rect(labelX - 12, y - 4, 640, rowH - 2, Color(0.9f, 0.15f, 0.2f, 0.3f));
        Color col = sel ? GOLD : WHITE;
        std::string label, value;
        if (r == ROW_MASTER) { label = "MASTER VOLUME"; value = bar(y, settings_.master, 10); }
        else if (r == ROW_MUSIC) { label = "MUSIC VOLUME"; value = bar(y, settings_.music, 10); }
        else if (r == ROW_SFX) { label = "EFFECTS VOLUME"; value = bar(y, settings_.sfx, 10); }
        else if (r == ROW_FULLSCREEN) { label = "FULLSCREEN"; value = settings_.fullscreen ? "<  ON  >" : "<  OFF  >"; }
        else if (r == ROW_VSYNC) { label = "VSYNC"; value = settings_.vsync ? "<  ON  >" : "<  OFF  >"; }
        else if (r == ROW_MOUSE) { label = "MOUSE SENSITIVITY"; value = bar(y, settings_.mouseSens, 10); }
        else if (r == ROW_RESET) { label = "RESET CONTROLS TO DEFAULT"; }
        else if (r == ROW_BACK) { label = "BACK"; }
        else {
            Action a = (Action)(r - ROW_KEYS);
            renderer_.text(labelX, y, actionName(a), 1.7f, col);
            for (int s = 0; s < BIND_SLOTS; ++s) {
                bool active = sel && s == settingsSlot_;
                bool capturing = active && settingsCapturing_;
                std::string t = capturing ? "PRESS A KEY..." : keyName(settings_.keys[(int)a][s]);
                Color c = capturing ? RED * (0.7f + 0.3f * std::sin(time_ * 10)) : (active ? GOLD : (sel ? WHITE : DIM));
                if (active) renderer_.rectOutline(valueX + s * 190 - 6, y - 4, 180, rowH - 2, 2, c);
                renderer_.text(valueX + s * 190, y, t, 1.7f, c);
            }
            continue;
        }
        renderer_.text(labelX, y, label, 1.7f, col);
        if (!value.empty()) renderer_.text(valueX, y, value, 1.7f, col);
    }
    std::string hint = settingsCapturing_ ? "PRESS THE NEW KEY     ESC  CANCEL"
        : "UP / DOWN  SELECT     LEFT / RIGHT  CHANGE     ENTER  REBIND / TOGGLE     BACKSPACE  CLEAR KEY     ESC  BACK";
    renderer_.textCentered(cx, height_ - 36, hint, 1.4f, DIM);
    renderer_.endFrame();
}

} // namespace dc
