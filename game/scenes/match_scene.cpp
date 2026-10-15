#include "scenes/match_scene.h"

#include "controls/bindings.h"
#include "core/overloaded.h"
#include "data/game_data.h"
#include "presentation/debug_view.h"
#include "sim/debug_names.h"
#include "sim/simulation.h"
#include "sim/state_hash.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <variant>

namespace dc::scenes {
namespace {

constexpr std::uint64_t aiSeedSalt = 0x9e3779b97f4a7c15ULL;

}  // namespace

MatchScene::MatchScene(SceneContext& context, MatchSetup setup)
    : setup_{std::move(setup)},
      data_{data::loadMatchData(context.assetsRoot, setup_.fighterP1, setup_.fighterP2)},
      state_{sim::makeInitialState(data_, setup_.seed)},
      previous_{state_},
      fightAudio_{context.audio} {
    if (setup_.p2IsAi) {
        ai_.emplace(sim::PlayerIndex::P2, ai::AiProfile::forDifficulty(setup_.aiDifficulty), setup_.seed ^ aiSeedSalt);
    }
    spdlog::info("Матч: {} против {}", data_.characters[0].displayName, data_.characters[1].displayName);
}

void MatchScene::fixedUpdate(SceneContext& context) {
    if (paused_ && !stepRequested_) {
        return;
    }
    stepRequested_ = false;

    const input::RawInputState& raw = context.input.state();
    sim::StepInput input;
    input.players[0] = controls::readPlayer(raw, sim::PlayerIndex::P1);
    input.players[1] = ai_ ? ai_->think(state_) : controls::readPlayer(raw, sim::PlayerIndex::P2);

    previous_ = state_;
    events_.clear();
    sim::step(state_, data_, input, events_);
    dispatchEvents();
}

void MatchScene::update(SceneContext& context, float frameSeconds, float /*alpha*/) {
    const input::RawInputState& raw = context.input.state();
    if (raw.wasPressed(GLFW_KEY_F1)) {
        showBoxes_ = !showBoxes_;
    }
    if (raw.wasPressed(GLFW_KEY_F2)) {
        paused_ = !paused_;
    }
    if (raw.wasPressed(GLFW_KEY_F3) && paused_) {
        stepRequested_ = true;
    }

    for (std::size_t i = 0; i < fighterViews_.size(); ++i) {
        fighterViews_[i].update(state_.fighters[i], data_.characters[i]);
    }

    const platform::FramebufferSize size = context.window.framebufferSize();
    const float aspect =
        size.height > 0 ? static_cast<float>(size.width) / static_cast<float>(size.height) : 16.0f / 9.0f;
    camera_.update(state_, frameSeconds, aspect);
    hud_.update(frameSeconds);
}

void MatchScene::render(SceneContext& /*context*/, gfx::RenderFrame& frame, float alpha) {
    frame.camera = camera_.cameraData();
    if (showBoxes_) {
        presentation::addMatchDebugShapes(previous_, state_, data_, paused_ ? 1.0f : alpha, frame);
    }
    // TODO(§7, §8.4): арена, модели бойцов из FighterView, частицы, HUD через dc_ui.
}

void MatchScene::debugUi(SceneContext& /*context*/) {
    ImGui::SetNextWindowPos(ImVec2{20.0f, 250.0f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Матч", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Тик %u · хеш %016llx", state_.frame, static_cast<unsigned long long>(sim::hashState(state_)));
    ImGui::Checkbox("Пауза (F2)", &paused_);
    ImGui::SameLine();
    if (ImGui::Button("Шаг (F3)")) {
        stepRequested_ = true;
    }
    ImGui::Checkbox("Боксы (F1)", &showBoxes_);
    for (std::size_t i = 0; i < state_.fighters.size(); ++i) {
        const sim::FighterState& fighter = state_.fighters[i];
        const std::string_view clip = fighterViews_[i].currentClip();
        ImGui::Text("P%zu: %s, кадр %d, x = %d мм, y = %d мм", i + 1, sim::toString(fighter.action),
                    fighter.actionFrame, fighter.position.x, fighter.position.y);
        ImGui::Text("     клип: %.*s", static_cast<int>(clip.size()), clip.data());
    }
    ImGui::End();

    hud_.drawDebug(state_, data_);
}

void MatchScene::dispatchEvents() {
    for (const sim::GameEvent& event : events_) {
        vfx_.onEvent(event);
        fightAudio_.onEvent(event);
        cinematics_.onEvent(event);
        hud_.onEvent(event);

        std::visit(core::Overloaded{
                       [](const sim::RoundStarted& started) { spdlog::info("Раунд {}", started.round); },
                       [](const sim::FightStarted&) { spdlog::info("FIGHT!"); },
                       [](const sim::RoundEnded& ended) {
                           spdlog::info("Раунд за P{}{}", sim::toIndex(ended.winner) + 1,
                                        ended.timeout ? " (время вышло)" : "");
                       },
                       [](const sim::MatchEnded& ended) {
                           spdlog::info("Матч за P{}", sim::toIndex(ended.winner) + 1);
                       },
                       [](const auto&) {},
                   },
                   event);
    }
}

}  // namespace dc::scenes
