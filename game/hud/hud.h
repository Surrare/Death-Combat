#pragma once

#include "sim/events.h"
#include "sim/match_data.h"
#include "sim/match_state.h"

#include <string>

namespace dc::hud {

// HUD боя (docs/ARCHITECTURE.md §10.3). Пока это отладочное окно ImGui с теми же данными:
// логика анонсов готова раньше, чем появится настоящий рендер интерфейса.
// TODO(§10.3, §10.4): полосы здоровья с «призрачным» уроном, медальоны раундов, таймер, метры, счётчик комбо,
//                     анонсы — на dc_ui после раунда impeccable new-work (неделя 10).
class Hud {
public:
    void onEvent(const sim::GameEvent& event);
    void update(float frameSeconds) noexcept;
    void drawDebug(const sim::MatchState& state, const sim::MatchData& data) const;

private:
    void announce(std::string text, float seconds);

    std::string announcement_;
    float announcementSeconds_{0.0f};
};

}  // namespace dc::hud
