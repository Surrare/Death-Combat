#pragma once

#include "audio/audio_engine.h"
#include "gfx/render_frame.h"
#include "input/raw_input.h"
#include "platform/window.h"

#include <filesystem>

namespace dc::scenes {

// Что сцена получает от App. Ссылки не владеют: всё это живёт в App дольше любой сцены.
struct SceneContext {
    platform::Window& window;
    input::InputDevices& input;
    audio::AudioEngine& audio;
    std::filesystem::path assetsRoot;
};

// Сцена — экран игры (docs/ARCHITECTURE.md §10.1).
// TODO(§10.1): Title, MainMenu, CharacterSelect, ArenaSelect, Loading, Versus, Results, Options, Training.
class Scene {
public:
    virtual ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    virtual void onEnter(SceneContext& /*context*/) {}
    virtual void onExit(SceneContext& /*context*/) {}
    virtual void fixedUpdate(SceneContext& context) = 0;  // ровно 60 раз в секунду
    virtual void update(SceneContext& context, float frameSeconds, float alpha) = 0;
    virtual void render(SceneContext& context, gfx::RenderFrame& frame, float alpha) = 0;
    virtual void debugUi(SceneContext& /*context*/) {}  // отладочные окна ImGui

protected:
    Scene() = default;
};

}  // namespace dc::scenes
