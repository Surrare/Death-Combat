#include "hud/hud.h"

#include "core/overloaded.h"
#include "sim/debug_names.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <variant>

namespace dc::hud {

void Hud::onEvent(const sim::GameEvent& event) {
    std::visit(core::Overloaded{
                   [this](const sim::RoundStarted& started) {
                       announce("ROUND " + std::to_string(started.round), 1.5f);
                   },
                   [this](const sim::FightStarted&) { announce("FIGHT!", 1.0f); },
                   [this](const sim::FinishHim&) { announce("FINISH HIM!", 3.0f); },
                   [this](const sim::RoundEnded& ended) {
                       announce(ended.timeout ? "TIME!" : "K.O.", 2.0f);
                   },
                   [this](const sim::MatchEnded& ended) {
                       announce("PLAYER " + std::to_string(sim::toIndex(ended.winner) + 1) + " WINS!", 5.0f);
                   },
                   [](const auto&) {},
               },
               event);
}

void Hud::update(float frameSeconds) noexcept {
    if (announcementSeconds_ > 0.0f) {
        announcementSeconds_ -= frameSeconds;
        if (announcementSeconds_ <= 0.0f) {
            announcement_.clear();
        }
    }
}

void Hud::drawDebug(const sim::MatchState& state, const sim::MatchData& data) const {
    ImGui::SetNextWindowPos(ImVec2{20.0f, 20.0f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("HUD (временный)", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    const int seconds = (state.round.timerFrames + sim::ticksPerSecond - 1) / sim::ticksPerSecond;
    ImGui::Text("Раунд %d · %s · таймер %02d", state.round.number, sim::toString(state.round.phase), seconds);

    for (std::size_t i = 0; i < state.fighters.size(); ++i) {
        const sim::FighterState& fighter = state.fighters[i];
        const sim::CharacterData& character = data.characters[i];
        const float health =
            static_cast<float>(fighter.health) / static_cast<float>(std::max(character.stats.health, 1));
        ImGui::Text("%s · раундов выиграно: %d", character.displayName.c_str(), fighter.roundsWon);
        ImGui::ProgressBar(health, ImVec2{-1.0f, 0.0f});
    }
    if (!announcement_.empty()) {
        ImGui::TextColored(ImVec4{1.0f, 0.85f, 0.2f, 1.0f}, "%s", announcement_.c_str());
    }
    ImGui::End();
}

void Hud::announce(std::string text, float seconds) {
    announcement_ = std::move(text);
    announcementSeconds_ = seconds;
}

}  // namespace dc::hud
