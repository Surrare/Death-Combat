#pragma once

#include "sim/events.h"
#include "sim/input_frame.h"
#include "sim/match_data.h"
#include "sim/match_state.h"

#include <array>
#include <cstdint>

namespace dc::sim {

struct StepInput {
    std::array<InputFrame, 2> players{};
};

// Начальное состояние матча: бойцы на стартовых позициях, раунд 1, фаза Intro.
[[nodiscard]] MatchState makeInitialState(const MatchData& data, std::uint64_t seed) noexcept;

// Один тик 1/60 с (docs/ARCHITECTURE.md §4.3). Зависит только от состояния, ввода и неизменяемых данных:
// никакого GL, звука и системного времени. Порядок шагов внутри фиксирован.
void step(MatchState& state, const MatchData& data, const StepInput& input, EventList& events) noexcept;

}  // namespace dc::sim
