#include "app/app.h"

#include "core/file_io.h"
#include "scenes/match_scene.h"
#include "sim/types.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace dc::game {
namespace {

constexpr double maxFrameSeconds = 0.25;       // после паузы или отладчика не пытаемся догнать сотни тиков
constexpr double vsyncSnapTolerance = 0.0002;  // 0,2 мс

platform::WindowDesc makeWindowDesc() {
    platform::WindowDesc desc;
#ifndef NDEBUG
    desc.debugContext = true;
#endif
    return desc;
}

// Если кадр почти ровно равен периоду монитора — считаем, что ровно. Убирает дрожание
// «0 или 2 тика за кадр» при vsync (docs/ARCHITECTURE.md §3).
double snapToRefreshRate(double frameSeconds, int refreshRate) noexcept {
    if (refreshRate <= 0) {
        return frameSeconds;
    }
    const double vsyncSeconds = 1.0 / static_cast<double>(refreshRate);
    return std::abs(frameSeconds - vsyncSeconds) < vsyncSnapTolerance ? vsyncSeconds : frameSeconds;
}

}  // namespace

App::App()
    : window_{makeWindowDesc()},
      imgui_{window_.handle()},
      assetsRoot_{core::assetsDirectory()},
      glRenderer_{reinterpret_cast<const char*>(glGetString(GL_RENDERER))},
      context_{window_, input_, audio_, assetsRoot_} {
    spdlog::info("Ассеты: {}", core::pathToUtf8(assetsRoot_));
    // TODO(§10.1): начинать с Title → MainMenu; пока сразу «Быстрый бой» против ИИ.
    scenes_.push(std::make_unique<scenes::MatchScene>(context_, scenes::MatchSetup{}));
    scenes_.applyPending(context_);
}

void App::run() {
    constexpr double tickSeconds = 1.0 / static_cast<double>(sim::ticksPerSecond);
    double previous = clock_.now();
    double accumulator = 0.0;

    while (!window_.shouldClose()) {
        const double now = clock_.now();
        const double frameSeconds = std::min(snapToRefreshRate(now - previous, window_.refreshRate()), maxFrameSeconds);
        previous = now;
        accumulator += frameSeconds;

        window_.pollEvents();
        input_.poll(window_.handle());
        if (input_.state().wasPressed(GLFW_KEY_ESCAPE)) {
            window_.requestClose();  // TODO(§10.1): Esc открывает паузу-оверлей, а не закрывает игру
        }

        scenes_.applyPending(context_);
        scenes::Scene* scene = scenes_.top();
        if (scene == nullptr) {
            window_.requestClose();
            continue;
        }

        while (accumulator >= tickSeconds) {
            scene->fixedUpdate(context_);  // ввод → sim::step → события → представление
            accumulator -= tickSeconds;
        }
        const float alpha = static_cast<float>(accumulator / tickSeconds);
        scene->update(context_, static_cast<float>(frameSeconds), alpha);

        gfx::RenderFrame frame;
        scene->render(context_, frame, alpha);
        const platform::FramebufferSize size = window_.framebufferSize();
        if (size.width > 0 && size.height > 0) {  // свёрнутое окно не рисуем
            renderer_.render(frame, size.width, size.height);
        }

        imgui_.beginFrame();
        drawStats(frameSeconds);
        scene->debugUi(context_);
        imgui_.endFrame();

        window_.swapBuffers();
    }
}

void App::drawStats(double frameSeconds) const {
    // Правый верхний угол: pivot (1, 0) прижимает окно правым краем.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2{viewport->WorkPos.x + viewport->WorkSize.x - 20.0f, viewport->WorkPos.y + 20.0f},
                            ImGuiCond_FirstUseEver, ImVec2{1.0f, 0.0f});
    ImGui::Begin("Статистика", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("%.1f FPS · %.2f мс", frameSeconds > 0.0 ? 1.0 / frameSeconds : 0.0, frameSeconds * 1000.0);
    ImGui::TextUnformatted(glRenderer_.c_str());
    ImGui::TextUnformatted("F1 — боксы · F2 — пауза · F3 — шаг · Esc — выход");
    ImGui::End();
}

}  // namespace dc::game
