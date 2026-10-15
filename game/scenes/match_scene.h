#pragma once

#include "ai/ai_controller.h"
#include "hud/hud.h"
#include "presentation/cinematic_player.h"
#include "presentation/fight_audio.h"
#include "presentation/fight_camera.h"
#include "presentation/fighter_view.h"
#include "presentation/vfx_system.h"
#include "scenes/scene.h"
#include "sim/events.h"
#include "sim/match_data.h"
#include "sim/match_state.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace dc::scenes {

struct MatchSetup {
    std::string fighterP1{"fighter_red"};
    std::string fighterP2{"fighter_blue"};
    bool p2IsAi{true};
    ai::AiDifficulty aiDifficulty{ai::AiDifficulty::Normal};
    std::uint64_t seed{20261015};
};

// Матч (docs/ARCHITECTURE.md §10.2): владеет состоянием боя и всем представлением.
// Фазы раунда решает симуляция, сцена их только показывает.
class MatchScene final : public Scene {
public:
    MatchScene(SceneContext& context, MatchSetup setup);  // core::DataError, если данные бойцов битые

    void fixedUpdate(SceneContext& context) override;
    void update(SceneContext& context, float frameSeconds, float alpha) override;
    void render(SceneContext& context, gfx::RenderFrame& frame, float alpha) override;
    void debugUi(SceneContext& context) override;

private:
    void dispatchEvents();

    MatchSetup setup_;
    sim::MatchData data_;
    sim::MatchState state_;
    sim::MatchState previous_;  // состояние на прошлом тике — для интерполяции (§3)
    sim::EventList events_;
    std::optional<ai::AiController> ai_;
    std::array<presentation::FighterView, 2> fighterViews_;
    presentation::FightCamera camera_;
    presentation::VfxSystem vfx_;
    presentation::FightAudio fightAudio_;
    presentation::CinematicPlayer cinematics_;
    hud::Hud hud_;
    bool paused_{false};
    bool stepRequested_{false};
    bool showBoxes_{true};
};

}  // namespace dc::scenes
