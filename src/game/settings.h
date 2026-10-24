// Persistent user settings: audio levels, video options, mouse sensitivity and player 1 key bindings.
#pragma once
#include <string>

namespace dc {

// Rebindable player 1 actions (arrows stay player 2 / menu navigation, Enter and Esc stay fixed).
enum class Action { Up, Down, Left, Right, Light, Medium, Heavy, Style, Dash, Taunt, LookLeft, LookRight, Count };
constexpr int ACTION_COUNT = (int)Action::Count;
constexpr int BIND_SLOTS = 2;

const char* actionName(Action a);
std::string keyName(int glfwKey);   // upper-case label for a GLFW key code, "NONE" for 0

struct Settings {
    int master = 10, music = 8, sfx = 10;   // 0..10
    bool fullscreen = false;
    bool vsync = true;
    int mouseSens = 5;                      // 1..10, 5 = default
    int keys[ACTION_COUNT][BIND_SLOTS];     // GLFW key codes, 0 = unbound

    Settings() { resetKeys(); }
    void resetKeys();
    float mouseScale() const { return 0.12f * (float)mouseSens / 5.0f; }
    // Rebind: assigns key to slot, clearing it from any other action's slots.
    void bind(Action a, int slot, int key);
    std::string keyLabel(Action a) const;   // e.g. "J / NONE"

    bool load();                            // false if no file (defaults kept)
    bool save() const;
    static std::string path();
};

} // namespace dc
