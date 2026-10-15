#pragma once

#include "gfx/render_frame.h"
#include "sim/match_data.h"
#include "sim/match_state.h"

namespace dc::presentation {

// Отладочный вид боя (F1): пол, стены, пушбоксы и направление взгляда. Позиции интерполируются между
// двумя последними тиками (alpha), поэтому движение плавное на мониторе любой частоты (§3).
void addMatchDebugShapes(const sim::MatchState& previous, const sim::MatchState& current,
                         const sim::MatchData& data, float alpha, gfx::RenderFrame& frame);

}  // namespace dc::presentation
