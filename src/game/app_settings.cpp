// Settings screen: 4-tab layout (Gameplay, Audio & Video, Player 1 Controls, Player 2 Controls)
// with strict architectural styling, mouse + keyboard navigation, and 2-player keybinding support.
#include "game/app.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <string>

namespace dc {

namespace {
const Color WHITE(0.95f, 0.96f, 0.98f);
const Color GOLD(0.88f, 0.74f, 0.42f);
const Color RED(0.85f, 0.24f, 0.26f);
const Color DIM(0.56f, 0.59f, 0.65f);
const Color UI_PANEL(0.035f, 0.04f, 0.055f, 0.95f);
const Color UI_PANEL_HI(0.11f, 0.13f, 0.18f, 0.96f);
const Color UI_BORDER(0.22f, 0.25f, 0.32f, 0.88f);
const Color UI_SUBTEXT(0.68f, 0.72f, 0.78f, 1.0f);

constexpr int TAB_COUNT = 4;
const char* TAB_NAMES[TAB_COUNT] = {
    "GAMEPLAY",
    "AUDIO & VIDEO",
    "PLAYER 1",
    "PLAYER 2"
};

const char* RENDER_MODE_NAMES[] = {"FULL", "DIRECT", "NO BLOOM", "NO OUTLINE", "NO SHADOWS"};
const char* DIFFICULTY_NAMES[] = {"EASY", "NORMAL", "HARD"};
const char* WINDOW_PRESET_NAMES[] = {"1280 x 720", "1600 x 900", "1920 x 1080"};
const int WINDOW_PRESET_W[] = {1280, 1600, 1920};
const int WINDOW_PRESET_H[] = {720, 900, 1080};

enum GameplayRow {
    GP_LANGUAGE = 0,
    GP_ROUNDS,
    GP_TIMER,
    GP_DIFFICULTY,
    GP_DAMAGE,
    GP_SHAKE,
    GP_HITSTOP,
    GP_HUD_HINTS,
    GP_DEV_MODE,
    GP_RESET_ALL,
    GP_BACK,
    GP_ROW_COUNT
};

enum AvRow {
    AV_MASTER = 0,
    AV_MUSIC,
    AV_SFX,
    AV_FULLSCREEN,
    AV_RESOLUTION,
    AV_VSYNC,
    AV_RENDER_MODE,
    AV_FOV,
    AV_MOUSE_SENS,
    AV_BACK,
    AV_ROW_COUNT
};

// Player 1 & Player 2 tab rows:
// Row 0: Easy Beat Auto-Combo toggle
// Rows 1 .. ACTION_COUNT: Key bindings (2 slots each)
// Row ACTION_COUNT + 1: Reset Player Keys
// Row ACTION_COUNT + 2: Back
constexpr int PL_EASY_CONTROLS = 0;
constexpr int PL_KEY_FIRST = 1;
constexpr int PL_KEY_LAST = PL_KEY_FIRST + ACTION_COUNT - 1;
constexpr int PL_RESET_KEYS = PL_KEY_FIRST + ACTION_COUNT;
constexpr int PL_BACK = PL_RESET_KEYS + 1;
constexpr int PL_ROW_COUNT = PL_BACK + 1;

int tabRowCount(int tab) {
    if (tab == 0) return GP_ROW_COUNT;
    if (tab == 1) return AV_ROW_COUNT;
    return PL_ROW_COUNT;
}

struct SettingsLayout {
    float panelX, panelY, panelW, panelH;
    float tabY, tabH, tabW, tabGap, tabX0;
    float contentY0, rowH, labelX, valueX, rowW;
};

SettingsLayout calcLayout(int width, int height, int tab) {
    SettingsLayout L{};
    L.panelW = std::min(880.0f, width - 48.0f);
    L.panelH = std::min(676.0f, height - 24.0f);
    L.panelX = (width - L.panelW) * 0.5f;
    L.panelY = (height - L.panelH) * 0.5f;

    L.tabY = L.panelY + 58.0f;
    L.tabH = 36.0f;
    L.tabGap = 10.0f;
    L.tabW = (L.panelW - 56.0f - L.tabGap * (TAB_COUNT - 1)) / TAB_COUNT;
    L.tabX0 = L.panelX + 28.0f;

    L.contentY0 = L.tabY + L.tabH + 22.0f;
    int rows = tabRowCount(tab);
    float availH = (L.panelY + L.panelH - 54.0f) - L.contentY0;
    L.rowH = std::clamp(availH / (float)rows, 26.0f, 38.0f);
    L.labelX = L.panelX + 40.0f;
    L.valueX = L.panelX + L.panelW * 0.48f;
    L.rowW = L.panelW - 56.0f;
    return L;
}
} // namespace

void App::openSettings(Scene returnTo) {
    settingsReturn_ = returnTo;
    settingsIndex_ = 0;
    settingsSlot_ = 0;
    settingsCapturing_ = false;
    scene_ = Scene::Settings;
}

void App::applyVideoSettings() {
    glfwSwapInterval(settings_.vsync ? 1 : 0);
    bool isFull = glfwGetWindowMonitor(window_) != nullptr;
    int preset = std::clamp(settings_.windowPreset, 0, 2);
    windowedW_ = WINDOW_PRESET_W[preset];
    windowedH_ = WINDOW_PRESET_H[preset];

    if (settings_.fullscreen && !isFull) {
        glfwGetWindowPos(window_, &windowedX_, &windowedY_);
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* vm = mon ? glfwGetVideoMode(mon) : nullptr;
        if (vm) glfwSetWindowMonitor(window_, mon, 0, 0, vm->width, vm->height, vm->refreshRate);
    } else if (!settings_.fullscreen && isFull) {
        glfwSetWindowMonitor(window_, nullptr, windowedX_, windowedY_, windowedW_, windowedH_, 0);
    } else if (!settings_.fullscreen && !isFull) {
        int curW = 0, curH = 0;
        glfwGetWindowSize(window_, &curW, &curH);
        if (curW != windowedW_ || curH != windowedH_) {
            glfwSetWindowSize(window_, windowedW_, windowedH_);
        }
    }
}

void App::closeSettings() {
    settings_.save();
    scene_ = settingsReturn_;
}

void App::updateSettings(float /*dt*/) {
    if (input_.mouseCaptured()) input_.setMouseCaptured(false);
    const PadState& m = input_.menu();

    if (settingsCapturing_) {
        int k = input_.newKey();
        if (k == GLFW_KEY_ESCAPE || input_.rightClicked()) {
            settingsCapturing_ = false;
        } else if (k) {
            int player = (settingsTab_ == 3) ? 1 : 0;
            Action act = (Action)(settingsIndex_ - PL_KEY_FIRST);
            settings_.bind(act, settingsSlot_, k, player);
            settingsCapturing_ = false;
        }
        return;
    }

    // Tab switching via Q / E / Tab keys or clicking a tab button
    int kNew = input_.newKey();
    if (kNew == GLFW_KEY_TAB || kNew == GLFW_KEY_E || kNew == GLFW_KEY_PAGE_DOWN) {
        settingsTab_ = (settingsTab_ + 1) % TAB_COUNT;
        settingsIndex_ = std::min(settingsIndex_, tabRowCount(settingsTab_) - 1);
    } else if (kNew == GLFW_KEY_Q || kNew == GLFW_KEY_PAGE_UP) {
        settingsTab_ = (settingsTab_ + TAB_COUNT - 1) % TAB_COUNT;
        settingsIndex_ = std::min(settingsIndex_, tabRowCount(settingsTab_) - 1);
    }

    SettingsLayout L = calcLayout(width_, height_, settingsTab_);

    // Check mouse clicks on tabs
    if (input_.leftClicked()) {
        for (int t = 0; t < TAB_COUNT; ++t) {
            float tx = L.tabX0 + t * (L.tabW + L.tabGap);
            if (input_.mouseIn(tx, L.tabY, L.tabW, L.tabH)) {
                settingsTab_ = t;
                L = calcLayout(width_, height_, settingsTab_);
                settingsIndex_ = std::min(settingsIndex_, tabRowCount(settingsTab_) - 1);
                return;
            }
        }
    }

    int rows = tabRowCount(settingsTab_);
    bool clicked = false;
    int clickDx = 0;

    // Mouse hover and click on rows
    for (int r = 0; r < rows; ++r) {
        float ry = L.contentY0 + r * L.rowH;
        if (!input_.mouseIn(L.panelX + 28.0f, ry, L.rowW, L.rowH - 2.0f)) continue;
        if (input_.mouseMoved() || input_.leftClicked()) settingsIndex_ = r;
        if (!input_.leftClicked()) break;

        float mx = input_.mouseX();
        if ((settingsTab_ == 2 || settingsTab_ == 3) && r >= PL_KEY_FIRST && r <= PL_KEY_LAST) {
            if (mx >= L.valueX) {
                float slotW = (L.panelX + L.panelW - 36.0f - L.valueX) * 0.5f;
                settingsSlot_ = (mx >= L.valueX + slotW) ? 1 : 0;
                clicked = true;
            }
        } else if (settingsTab_ == 1 && (r == AV_MASTER || r == AV_MUSIC || r == AV_SFX || r == AV_MOUSE_SENS)) {
            int& v = (r == AV_MASTER) ? settings_.master :
                     (r == AV_MUSIC)  ? settings_.music :
                     (r == AV_SFX)    ? settings_.sfx : settings_.mouseSens;
            int lo = (r == AV_MOUSE_SENS) ? 1 : 0;
            float barX = L.valueX + 28.0f;
            float barW = 200.0f;
            if (mx >= barX && mx <= barX + barW) {
                int step = std::clamp((int)(((mx - barX) / barW) * 10.0f + 0.5f), lo, 10);
                v = step;
            } else if (mx >= L.valueX && mx < barX) {
                clickDx = -1;
            } else if (mx > barX + barW && mx <= barX + barW + 40.0f) {
                clickDx = 1;
            }
        } else {
            if (mx >= L.valueX && mx < L.valueX + 44.0f) clickDx = -1;
            else if (mx > L.valueX + 180.0f) clickDx = 1;
            else clicked = true;
        }
        break;
    }

    if (m.justPressed(BTN_DOWN)) settingsIndex_ = (settingsIndex_ + 1) % rows;
    if (m.justPressed(BTN_UP)) settingsIndex_ = (settingsIndex_ + rows - 1) % rows;

    int dx = (m.justPressed(BTN_RIGHT) ? 1 : 0) - (m.justPressed(BTN_LEFT) ? 1 : 0) + clickDx;
    bool confirm = m.justPressed(BTN_START) || m.justPressed(BTN_LIGHT) || clicked;
    int r = settingsIndex_;

    auto stepInt = [&](int& v, int lo, int hi, int step = 1) {
        if (dx) v = std::clamp(v + dx * step, lo, hi);
        else if (confirm) v = (v + step > hi) ? lo : (v + step);
    };

    if (settingsTab_ == 0) {
        // GAMEPLAY TAB
        if (r == GP_LANGUAGE && (dx || confirm)) {
            int lang = static_cast<int>(settings_.language);
            settings_.language = static_cast<Language>((lang + (dx < 0 ? 2 : 1)) % 3);
        } else if (r == GP_ROUNDS) {
            stepInt(settings_.roundsToWin, 1, 5, 1);
        } else if (r == GP_TIMER && (dx || confirm)) {
            settings_.roundTimeIndex = (settings_.roundTimeIndex + (dx < 0 ? 3 : 1)) % 4;
        } else if (r == GP_DIFFICULTY && (dx || confirm)) {
            settings_.aiDifficulty = (settings_.aiDifficulty + (dx < 0 ? 2 : 1)) % 3;
        } else if (r == GP_DAMAGE) {
            stepInt(settings_.damageScalePct, 75, 150, 25);
        } else if (r == GP_SHAKE) {
            stepInt(settings_.screenShakePct, 0, 100, 50);
        } else if (r == GP_HITSTOP && (dx || confirm)) {
            settings_.hitstopEnabled = !settings_.hitstopEnabled;
        } else if (r == GP_HUD_HINTS && (dx || confirm)) {
            settings_.showHudPrompts = !settings_.showHudPrompts;
        } else if (r == GP_DEV_MODE && (dx || confirm)) {
            settings_.devMode = !settings_.devMode;
        } else if (r == GP_RESET_ALL && confirm) {
            settings_ = Settings{};
            applyVideoSettings();
        } else if (r == GP_BACK && confirm) {
            closeSettings();
            return;
        }
    } else if (settingsTab_ == 1) {
        // AUDIO & VIDEO TAB
        if (r == AV_MASTER) stepInt(settings_.master, 0, 10, 1);
        else if (r == AV_MUSIC) stepInt(settings_.music, 0, 10, 1);
        else if (r == AV_SFX) stepInt(settings_.sfx, 0, 10, 1);
        else if (r == AV_FULLSCREEN && (dx || confirm)) {
            settings_.fullscreen = !settings_.fullscreen;
            applyVideoSettings();
        } else if (r == AV_RESOLUTION && (dx || confirm)) {
            settings_.windowPreset = (settings_.windowPreset + (dx < 0 ? 2 : 1)) % 3;
            applyVideoSettings();
        } else if (r == AV_VSYNC && (dx || confirm)) {
            settings_.vsync = !settings_.vsync;
            applyVideoSettings();
        } else if (r == AV_RENDER_MODE && (dx || confirm)) {
            settings_.renderMode = (settings_.renderMode + (dx < 0 ? 4 : 1)) % 5;
        } else if (r == AV_FOV) {
            stepInt(settings_.fovDeg, 45, 70, 5);
        } else if (r == AV_MOUSE_SENS) {
            stepInt(settings_.mouseSens, 1, 10, 1);
        } else if (r == AV_BACK && confirm) {
            closeSettings();
            return;
        }
    } else {
        // PLAYER 1 (tab 2) or PLAYER 2 (tab 3)
        int player = (settingsTab_ == 3) ? 1 : 0;
        auto& keyTable = (player == 0) ? settings_.keys : settings_.keysP2;
        if (r == PL_EASY_CONTROLS && (dx || confirm)) {
            settings_.easyControls[player] = !settings_.easyControls[player];
        } else if (r >= PL_KEY_FIRST && r <= PL_KEY_LAST) {
            if (dx) settingsSlot_ = std::clamp(settingsSlot_ + dx, 0, BIND_SLOTS - 1);
            if (confirm) settingsCapturing_ = true;
            if (glfwGetKey(window_, GLFW_KEY_BACKSPACE) == GLFW_PRESS ||
                glfwGetKey(window_, GLFW_KEY_DELETE) == GLFW_PRESS) {
                keyTable[r - PL_KEY_FIRST][settingsSlot_] = 0;
            }
        } else if (r == PL_RESET_KEYS && confirm) {
            if (player == 0) settings_.resetKeysP1();
            else settings_.resetKeysP2();
        } else if (r == PL_BACK && confirm) {
            closeSettings();
            return;
        }
    }

    if (m.justPressed(BTN_BACK) || input_.rightClicked()) closeSettings();
}

void App::renderSettings() {
    renderer_.beginFrame(Camera(), Color(0.02f, 0.024f, 0.032f));
    const SettingsLayout L = calcLayout(width_, height_, settingsTab_);
    const float cx = width_ * 0.5f;

    // Outer architectural frame
    renderer_.rect(L.panelX, L.panelY, L.panelW, L.panelH, UI_PANEL);
    renderer_.rectOutline(L.panelX, L.panelY, L.panelW, L.panelH, 1.0f, UI_BORDER);
    renderer_.rect(L.panelX, L.panelY, L.panelW, 3.0f, GOLD);

    // Header
    renderer_.text(L.panelX + 28.0f, L.panelY + 18.0f, "SYSTEM CONFIGURATION", 2.4f, WHITE);
    renderer_.textRight(L.panelX + L.panelW - 28.0f, L.panelY + 22.0f, "Q / E / TAB  SWITCH CATEGORY", 1.35f, DIM);

    // Category Tabs
    for (int t = 0; t < TAB_COUNT; ++t) {
        float tx = L.tabX0 + t * (L.tabW + L.tabGap);
        bool activeTab = (t == settingsTab_);
        renderer_.rect(tx, L.tabY, L.tabW, L.tabH, activeTab ? UI_PANEL_HI : Color(0.05f, 0.055f, 0.075f, 0.8f));
        renderer_.rectOutline(tx, L.tabY, L.tabW, L.tabH, 1.0f, activeTab ? GOLD : UI_BORDER);
        if (activeTab) {
            renderer_.rect(tx, L.tabY + L.tabH - 3.0f, L.tabW, 3.0f, GOLD);
        }
        renderer_.textCentered(tx + L.tabW * 0.5f, L.tabY + 10.0f, TAB_NAMES[t], 1.55f, activeTab ? GOLD : UI_SUBTEXT);
    }

    // Helper for clean graphical slider
    auto drawSlider = [&](float y, int val, int maxVal, bool sel, const std::string& suffix) {
        float barX = L.valueX + 28.0f;
        float barW = 200.0f;
        float barH = 10.0f;
        float barY = y + (L.rowH - barH) * 0.5f - 1.0f;
        Color accent = sel ? GOLD : WHITE;
        renderer_.text(L.valueX + 6.0f, y + 5.0f, "<", 1.5f, sel ? GOLD : DIM);
        renderer_.rect(barX, barY, barW, barH, Color(0.08f, 0.09f, 0.12f, 0.95f));
        renderer_.rectOutline(barX, barY, barW, barH, 1.0f, UI_BORDER);
        float fill = std::clamp((float)val / (float)maxVal, 0.0f, 1.0f);
        if (fill > 0.0f) {
            renderer_.rect(barX + 1.0f, barY + 1.0f, (barW - 2.0f) * fill, barH - 2.0f, accent);
        }
        renderer_.text(barX + barW + 12.0f, y + 5.0f, ">", 1.5f, sel ? GOLD : DIM);
        renderer_.text(barX + barW + 34.0f, y + 5.0f, suffix, 1.45f, accent);
    };

    auto drawSelector = [&](float y, const std::string& text, bool sel) {
        Color c = sel ? GOLD : WHITE;
        renderer_.text(L.valueX + 6.0f, y + 5.0f, "<", 1.5f, sel ? GOLD : DIM);
        renderer_.textCentered(L.valueX + 114.0f, y + 5.0f, text, 1.55f, c);
        renderer_.text(L.valueX + 214.0f, y + 5.0f, ">", 1.5f, sel ? GOLD : DIM);
    };

    int rows = tabRowCount(settingsTab_);
    for (int r = 0; r < rows; ++r) {
        float ry = L.contentY0 + r * L.rowH;
        bool sel = (r == settingsIndex_);

        // Row background
        renderer_.rect(L.panelX + 28.0f, ry, L.rowW, L.rowH - 3.0f,
                       sel ? UI_PANEL_HI : Color(0.045f, 0.05f, 0.068f, (r % 2 == 0) ? 0.65f : 0.42f));
        if (sel) {
            renderer_.rectOutline(L.panelX + 28.0f, ry, L.rowW, L.rowH - 3.0f, 1.0f, GOLD);
            renderer_.rect(L.panelX + 28.0f, ry, 3.0f, L.rowH - 3.0f, GOLD);
        }

        Color labelCol = sel ? GOLD : WHITE;

        if (settingsTab_ == 0) {
            // GAMEPLAY
            if (r == GP_LANGUAGE) {
                renderer_.text(L.labelX, ry + 5.0f, "LANGUAGE", 1.55f, labelCol);
                drawSelector(ry, languageName(settings_.language), sel);
            } else if (r == GP_ROUNDS) {
                renderer_.text(L.labelX, ry + 5.0f, "ROUNDS TO WIN", 1.55f, labelCol);
                drawSelector(ry, std::to_string(settings_.roundsToWin), sel);
            } else if (r == GP_TIMER) {
                renderer_.text(L.labelX, ry + 5.0f, "ROUND TIMER", 1.55f, labelCol);
                const char* tnames[] = {"60 SECONDS", "99 SECONDS", "120 SECONDS", "INFINITE"};
                drawSelector(ry, tnames[std::clamp(settings_.roundTimeIndex, 0, 3)], sel);
            } else if (r == GP_DIFFICULTY) {
                renderer_.text(L.labelX, ry + 5.0f, "CPU DIFFICULTY", 1.55f, labelCol);
                drawSelector(ry, DIFFICULTY_NAMES[std::clamp(settings_.aiDifficulty, 0, 2)], sel);
            } else if (r == GP_DAMAGE) {
                renderer_.text(L.labelX, ry + 5.0f, "DAMAGE SCALE", 1.55f, labelCol);
                drawSelector(ry, std::to_string(settings_.damageScalePct) + "%", sel);
            } else if (r == GP_SHAKE) {
                renderer_.text(L.labelX, ry + 5.0f, "CAMERA SHAKE", 1.55f, labelCol);
                drawSelector(ry, settings_.screenShakePct == 0 ? "OFF" : (std::to_string(settings_.screenShakePct) + "%"), sel);
            } else if (r == GP_HITSTOP) {
                renderer_.text(L.labelX, ry + 5.0f, "IMPACT HITSTOP", 1.55f, labelCol);
                drawSelector(ry, settings_.hitstopEnabled ? "ON" : "OFF", sel);
            } else if (r == GP_HUD_HINTS) {
                renderer_.text(L.labelX, ry + 5.0f, "HUD CONTROL HINTS", 1.55f, labelCol);
                drawSelector(ry, settings_.showHudPrompts ? "ON" : "OFF", sel);
            } else if (r == GP_DEV_MODE) {
                renderer_.text(L.labelX, ry + 5.0f, "DEVELOPER MODE (F3)", 1.55f, labelCol);
                drawSelector(ry, settings_.devMode ? "ENABLED" : "DISABLED", sel);
            } else if (r == GP_RESET_ALL) {
                renderer_.text(L.labelX, ry + 5.0f, "RESET ALL SETTINGS TO DEFAULT", 1.55f, sel ? GOLD : UI_SUBTEXT);
            } else if (r == GP_BACK) {
                renderer_.text(L.labelX, ry + 5.0f, "SAVE & RETURN", 1.55f, labelCol);
            }
        } else if (settingsTab_ == 1) {
            // AUDIO & VIDEO
            if (r == AV_MASTER) {
                renderer_.text(L.labelX, ry + 5.0f, "MASTER VOLUME", 1.55f, labelCol);
                drawSlider(ry, settings_.master, 10, sel, std::to_string(settings_.master * 10) + "%");
            } else if (r == AV_MUSIC) {
                renderer_.text(L.labelX, ry + 5.0f, "MUSIC VOLUME", 1.55f, labelCol);
                drawSlider(ry, settings_.music, 10, sel, std::to_string(settings_.music * 10) + "%");
            } else if (r == AV_SFX) {
                renderer_.text(L.labelX, ry + 5.0f, "EFFECTS VOLUME", 1.55f, labelCol);
                drawSlider(ry, settings_.sfx, 10, sel, std::to_string(settings_.sfx * 10) + "%");
            } else if (r == AV_FULLSCREEN) {
                renderer_.text(L.labelX, ry + 5.0f, "DISPLAY MODE", 1.55f, labelCol);
                drawSelector(ry, settings_.fullscreen ? "FULLSCREEN" : "WINDOWED", sel);
            } else if (r == AV_RESOLUTION) {
                renderer_.text(L.labelX, ry + 5.0f, "WINDOW SIZE", 1.55f, labelCol);
                drawSelector(ry, WINDOW_PRESET_NAMES[std::clamp(settings_.windowPreset, 0, 2)], sel);
            } else if (r == AV_VSYNC) {
                renderer_.text(L.labelX, ry + 5.0f, "VERTICAL SYNC (VSYNC)", 1.55f, labelCol);
                drawSelector(ry, settings_.vsync ? "ON" : "OFF", sel);
            } else if (r == AV_RENDER_MODE) {
                renderer_.text(L.labelX, ry + 5.0f, "RENDER PIPELINE", 1.55f, labelCol);
                drawSelector(ry, RENDER_MODE_NAMES[std::clamp(settings_.renderMode, 0, 4)], sel);
            } else if (r == AV_FOV) {
                renderer_.text(L.labelX, ry + 5.0f, "CAMERA FIELD OF VIEW", 1.55f, labelCol);
                drawSelector(ry, std::to_string(settings_.fovDeg) + " DEG", sel);
            } else if (r == AV_MOUSE_SENS) {
                renderer_.text(L.labelX, ry + 5.0f, "HUNT MOUSE SENSITIVITY", 1.55f, labelCol);
                drawSlider(ry, settings_.mouseSens, 10, sel, std::to_string(settings_.mouseSens));
            } else if (r == AV_BACK) {
                renderer_.text(L.labelX, ry + 5.0f, "SAVE & RETURN", 1.55f, labelCol);
            }
        } else {
            // PLAYER 1 or PLAYER 2 CONTROLS
            int player = (settingsTab_ == 3) ? 1 : 0;
            const auto& keyTable = (player == 0) ? settings_.keys : settings_.keysP2;
            if (r == PL_EASY_CONTROLS) {
                renderer_.text(L.labelX, ry + 4.0f, player == 0 ? "P1 EASY BEAT AUTO-COMBO" : "P2 EASY BEAT AUTO-COMBO", 1.45f, labelCol);
                drawSelector(ry, settings_.easyControls[player] ? "ON (AUTO CHAIN)" : "OFF (MANUAL)", sel);
            } else if (r >= PL_KEY_FIRST && r <= PL_KEY_LAST) {
                Action a = (Action)(r - PL_KEY_FIRST);
                renderer_.text(L.labelX, ry + 4.0f, actionName(a), 1.45f, labelCol);
                float slotW = (L.panelX + L.panelW - 36.0f - L.valueX) * 0.5f;
                for (int s = 0; s < BIND_SLOTS; ++s) {
                    float sx = L.valueX + s * slotW;
                    bool activeSlot = sel && (s == settingsSlot_);
                    bool capturing = activeSlot && settingsCapturing_;
                    std::string t = capturing ? "PRESS KEY..." : keyName(keyTable[(int)a][s]);
                    Color c = capturing ? RED : (activeSlot ? GOLD : (sel ? WHITE : UI_SUBTEXT));
                    if (activeSlot) {
                        renderer_.rect(sx, ry + 2.0f, slotW - 10.0f, L.rowH - 7.0f, Color(0.08f, 0.09f, 0.13f, 0.95f));
                        renderer_.rectOutline(sx, ry + 2.0f, slotW - 10.0f, L.rowH - 7.0f, 1.0f, c);
                    }
                    renderer_.text(sx + 10.0f, ry + 4.0f, t, 1.45f, c);
                }
            } else if (r == PL_RESET_KEYS) {
                renderer_.text(L.labelX, ry + 4.0f, player == 0 ? "RESET PLAYER 1 KEYS TO DEFAULT" : "RESET PLAYER 2 KEYS TO DEFAULT", 1.45f, sel ? GOLD : UI_SUBTEXT);
            } else if (r == PL_BACK) {
                renderer_.text(L.labelX, ry + 4.0f, "SAVE & RETURN", 1.45f, labelCol);
            }
        }
    }

    std::string hint = settingsCapturing_
        ? "PRESS NEW KEY TO BIND     ESC / RIGHT CLICK  CANCEL"
        : "Q/E/TAB: CATEGORY   UP/DOWN: SELECT   LEFT/RIGHT: ADJUST   ENTER: TOGGLE/REBIND   BACKSPACE: CLEAR   ESC: BACK";
    renderer_.textCentered(cx, L.panelY + L.panelH - 28.0f, hint, 1.22f, DIM);
    drawDevOverlay();
    renderer_.endFrame();
}

} // namespace dc
