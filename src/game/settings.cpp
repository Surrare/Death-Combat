#include "game/settings.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace dc {

const char* actionName(Action a) {
    static const char* n[] = {"MOVE UP", "MOVE DOWN", "MOVE LEFT", "MOVE RIGHT", "LIGHT ATTACK", "MEDIUM ATTACK",
                              "HEAVY ATTACK", "STAND / MODE", "DASH / EVADE", "TAUNT", "LOOK LEFT", "LOOK RIGHT"};
    return n[(int)a];
}

std::string keyName(int k) {
    if (k == 0) return "NONE";
    switch (k) {
    case GLFW_KEY_SPACE: return "SPACE";
    case GLFW_KEY_ENTER: return "ENTER";
    case GLFW_KEY_ESCAPE: return "ESC";
    case GLFW_KEY_TAB: return "TAB";
    case GLFW_KEY_BACKSPACE: return "BACKSPACE";
    case GLFW_KEY_UP: return "UP";
    case GLFW_KEY_DOWN: return "DOWN";
    case GLFW_KEY_LEFT: return "LEFT";
    case GLFW_KEY_RIGHT: return "RIGHT";
    case GLFW_KEY_LEFT_SHIFT: return "L SHIFT";
    case GLFW_KEY_RIGHT_SHIFT: return "R SHIFT";
    case GLFW_KEY_LEFT_CONTROL: return "L CTRL";
    case GLFW_KEY_RIGHT_CONTROL: return "R CTRL";
    case GLFW_KEY_LEFT_ALT: return "L ALT";
    case GLFW_KEY_RIGHT_ALT: return "R ALT";
    case GLFW_KEY_INSERT: return "INSERT";
    case GLFW_KEY_DELETE: return "DELETE";
    case GLFW_KEY_HOME: return "HOME";
    case GLFW_KEY_END: return "END";
    case GLFW_KEY_PAGE_UP: return "PAGE UP";
    case GLFW_KEY_PAGE_DOWN: return "PAGE DOWN";
    case GLFW_KEY_KP_ENTER: return "NUM ENTER";
    default: break;
    }
    if (k >= GLFW_KEY_F1 && k <= GLFW_KEY_F25) return "F" + std::to_string(k - GLFW_KEY_F1 + 1);
    if (k >= GLFW_KEY_KP_0 && k <= GLFW_KEY_KP_9) return "NUM " + std::to_string(k - GLFW_KEY_KP_0);
    if (const char* n = glfwGetKeyName(k, 0)) {
        std::string s = n;
        for (char& c : s) c = (char)std::toupper((unsigned char)c);
        return s;
    }
    if (k >= 32 && k < 127) return std::string(1, (char)k);   // GLFW codes are ASCII for printable keys
    return "KEY " + std::to_string(k);
}

void Settings::resetKeys() {
    for (auto& row : keys) row[0] = row[1] = 0;
    auto set = [&](Action a, int k0, int k1 = 0) { keys[(int)a][0] = k0; keys[(int)a][1] = k1; };
    set(Action::Up, GLFW_KEY_W);
    set(Action::Down, GLFW_KEY_S);
    set(Action::Left, GLFW_KEY_A);
    set(Action::Right, GLFW_KEY_D);
    set(Action::Light, GLFW_KEY_J);
    set(Action::Medium, GLFW_KEY_K);
    set(Action::Heavy, GLFW_KEY_L);
    set(Action::Style, GLFW_KEY_I);
    set(Action::Dash, GLFW_KEY_U, GLFW_KEY_LEFT_SHIFT);
    set(Action::Taunt, GLFW_KEY_O);
    set(Action::LookLeft, GLFW_KEY_Q);
    set(Action::LookRight, GLFW_KEY_R);
}

void Settings::bind(Action a, int slot, int key) {
    for (auto& row : keys)
        for (int& k : row) if (k == key) k = 0;
    keys[(int)a][slot] = key;
}

std::string Settings::keyLabel(Action a) const {
    const int* k = keys[(int)a];
    if (!k[1]) return keyName(k[0]);
    return keyName(k[0]) + " / " + keyName(k[1]);
}

std::string Settings::path() {
    namespace fs = std::filesystem;
    fs::path dir;
#ifdef _WIN32
    if (const char* a = std::getenv("APPDATA")) dir = fs::path(a) / "DeathCombat";
#else
    if (const char* x = std::getenv("XDG_CONFIG_HOME")) dir = fs::path(x) / "death-combat";
    else if (const char* h = std::getenv("HOME")) dir = fs::path(h) / ".config" / "death-combat";
#endif
    if (dir.empty()) dir = ".";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "settings.cfg").string();
}

bool Settings::save() const {
    std::ofstream f(path());
    if (!f) return false;
    f << "master=" << master << "\nmusic=" << music << "\nsfx=" << sfx << "\n";
    f << "fullscreen=" << (fullscreen ? 1 : 0) << "\nvsync=" << (vsync ? 1 : 0) << "\nmouse=" << mouseSens << "\n";
    for (int a = 0; a < ACTION_COUNT; ++a)
        f << "key" << a << "=" << keys[a][0] << "," << keys[a][1] << "\n";
    return (bool)f;
}

bool Settings::load() {
    std::ifstream f(path());
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        int n = std::atoi(v.c_str());
        if (k == "master") master = std::clamp(n, 0, 10);
        else if (k == "music") music = std::clamp(n, 0, 10);
        else if (k == "sfx") sfx = std::clamp(n, 0, 10);
        else if (k == "fullscreen") fullscreen = n != 0;
        else if (k == "vsync") vsync = n != 0;
        else if (k == "mouse") mouseSens = std::clamp(n, 1, 10);
        else if (k.rfind("key", 0) == 0) {
            int a = std::atoi(k.c_str() + 3);
            size_t comma = v.find(',');
            if (a >= 0 && a < ACTION_COUNT && comma != std::string::npos) {
                keys[a][0] = std::atoi(v.c_str());
                keys[a][1] = std::atoi(v.c_str() + comma + 1);
            }
        }
    }
    return true;
}

} // namespace dc
