// Persistent user settings: audio levels, video options, mouse sensitivity and player 1 key bindings.
#pragma once
#include "core/localization.h"
#include <string>

namespace dc {

// Rebindable player 1 actions (arrows stay player 2 / menu navigation, Enter and Esc stay fixed).
// New actions are appended so saved key indices stay valid.
enum class Action { Up, Down, Left, Right, Light, Medium, Heavy, Style, Dash, Taunt, LookLeft, LookRight, Jump, Crouch, Count };
constexpr int ACTION_COUNT = (int)Action::Count;
constexpr int BIND_SLOTS = 2;

const char* actionName(Action a);
std::string keyName(int glfwKey);   // upper-case label for a GLFW key code, "NONE" for 0

struct Settings {
    // Audio & Video
    int master = 10, music = 8, sfx = 10;   // 0..10
    bool fullscreen = false;
    int windowPreset = 1;                   // 0 = 1280x720, 1 = 1600x900, 2 = 1920x1080
    bool vsync = true;
    Language language = Language::English;
    int renderMode = 0;                     // RenderDebugMode value: full, direct, no bloom/outline/shadows
    int fovDeg = 50;                        // Fight camera base FOV (45..65)

    // Gameplay & Match Rules
    int roundsToWin = 2;                    // 1..3 (Best of 1, 3, 5)
    int roundTimeIndex = 1;                 // 0 = 60s, 1 = 99s, 2 = 120s, 3 = Infinite
    int aiDifficulty = 1;                   // 0 = Easy, 1 = Normal, 2 = Hard
    int damageScalePct = 100;               // 75, 100, 125, 150
    int screenShakePct = 100;               // 0, 50, 100, 150
    bool hitstopEnabled = true;
    bool showHudPrompts = true;
    bool devMode = false;                   // Developer Mode overlay & hitboxes

    // Per-Player Controls (Player 1 & Player 2)
    int mouseSens = 5;                      // 1..10, 5 = default
    bool easyControls[2] = {true, true};    // Easy Beat auto-combo per player
    int keys[ACTION_COUNT][BIND_SLOTS];     // Player 1 GLFW key codes, 0 = unbound
    int keysP2[ACTION_COUNT][BIND_SLOTS];   // Player 2 GLFW key codes, 0 = unbound

    Settings() { resetKeys(); }
    void resetKeys();
    void resetKeysP1();
    void resetKeysP2();
    float mouseScale() const { return 0.12f * (float)mouseSens / 5.0f; }
    int roundSeconds() const {
        static const int SEC[4] = {60, 99, 120, 0};
        return SEC[roundTimeIndex >= 0 && roundTimeIndex < 4 ? roundTimeIndex : 1];
    }
    float damageScale() const { return (float)damageScalePct / 100.0f; }
    float shakeScale() const { return (float)screenShakePct / 100.0f; }

    // Rebind: assigns key to slot for the given player (0 or 1), clearing it from that player's other actions.
    void bind(Action a, int slot, int key, int player = 0);
    std::string keyLabel(Action a, int player = 0) const;   // e.g. "J / NONE"

    bool load();                            // false if no file (defaults kept)
    bool save() const;
    static std::string path();
};

} // namespace dc
