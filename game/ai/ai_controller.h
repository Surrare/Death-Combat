#pragma once

#include "core/pcg32.h"
#include "sim/input_frame.h"
#include "sim/match_state.h"
#include "sim/types.h"

#include <cstdint>
#include <deque>

namespace dc::ai {

enum class AiDifficulty : std::uint8_t { Easy, Normal, Hard };

// Сложность — это параметры, а не разный код (docs/ARCHITECTURE.md §6).
struct AiProfile {
    std::int32_t reactionFrames{16};
    std::int32_t blockChancePercent{55};
    std::int32_t preferredDistance{1400};  // мм

    [[nodiscard]] static AiProfile forDifficulty(AiDifficulty difficulty) noexcept;
};

// ИИ — виртуальный контроллер (ADR-0009): видит состояние матча с задержкой реакции
// и выдаёт только InputFrame, как человек с геймпадом. Состояние симуляции он не меняет.
class AiController {
public:
    AiController(sim::PlayerIndex self, AiProfile profile, std::uint64_t seed);

    [[nodiscard]] sim::InputFrame think(const sim::MatchState& current);

private:
    sim::PlayerIndex self_;
    AiProfile profile_;
    core::Pcg32 rng_;  // свой генератор, не симуляционный
    std::deque<sim::MatchState> history_;
};

}  // namespace dc::ai
