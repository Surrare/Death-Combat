#pragma once

#include "audio/audio_engine.h"
#include "core/clock.h"
#include "gfx/imgui_layer.h"
#include "gfx/renderer.h"
#include "input/raw_input.h"
#include "platform/window.h"
#include "scenes/scene.h"
#include "scenes/scene_stack.h"

#include <filesystem>
#include <string>

namespace dc::game {

// Приложение: окно, рендер, ввод, звук, стек сцен и главный цикл с фиксированным тиком (docs/ARCHITECTURE.md §3).
// Порядок полей важен: окно (контекст GL) создаётся первым и уничтожается последним.
class App {
public:
    App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void run();

private:
    void drawStats(double frameSeconds) const;

    platform::Window window_;
    gfx::Renderer renderer_;
    gfx::ImGuiLayer imgui_;
    input::InputDevices input_;
    audio::AudioEngine audio_;
    std::filesystem::path assetsRoot_;
    std::string glRenderer_;
    scenes::SceneContext context_;
    scenes::SceneStack scenes_;
    core::Clock clock_;
};

}  // namespace dc::game
