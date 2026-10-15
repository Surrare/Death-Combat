#pragma once

#include "sim/match_data.h"
#include "sim/match_state.h"

namespace dc::sim {

// Пушбокс бойца для текущей стойки (docs/ARCHITECTURE.md §4.6). Нужен симуляции и отладочной отрисовке.
[[nodiscard]] const Box& currentPushbox(const FighterState& fighter, const CharacterData& character) noexcept;

// TODO(§4.6): currentHurtboxes — набор стойки + hurtboxOverrides приёма на текущем кадре;
//             activeHitboxes — hitboxes приёма, активные на fighter.actionFrame.

}  // namespace dc::sim
