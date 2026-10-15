#pragma once

#include "sim/match_state.h"

#include <cstdint>

namespace dc::sim {

// Хеш состояния для теста детерминизма и отладки (docs/ARCHITECTURE.md §11).
// Считается по полям, а не по байтам: в байтах структуры есть выравнивание с мусором.
// Добавили поле в MatchState — добавьте его и сюда, иначе тест детерминизма его не увидит.
[[nodiscard]] std::uint64_t hashState(const MatchState& state) noexcept;

}  // namespace dc::sim
