#pragma once

#include "sim/match_state.h"
#include "sim/types.h"

namespace dc::sim {

// Имена для отладочного оверлея и логов.
[[nodiscard]] const char* toString(Action action) noexcept;
[[nodiscard]] const char* toString(RoundPhase phase) noexcept;

}  // namespace dc::sim
