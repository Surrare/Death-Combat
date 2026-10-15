#include "ai/ai_controller.h"

#include <cstdlib>

namespace dc::ai {

AiProfile AiProfile::forDifficulty(AiDifficulty difficulty) noexcept {
    switch (difficulty) {
    case AiDifficulty::Easy: return AiProfile{26, 25, 1600};
    case AiDifficulty::Normal: return AiProfile{16, 55, 1400};
    case AiDifficulty::Hard: return AiProfile{9, 80, 1200};
    }
    return AiProfile{};
}

AiController::AiController(sim::PlayerIndex self, AiProfile profile, std::uint64_t seed)
    : self_{self}, profile_{profile}, rng_{seed} {}

sim::InputFrame AiController::think(const sim::MatchState& current) {
    history_.push_back(current);
    while (history_.size() > static_cast<std::size_t>(profile_.reactionFrames) + 1) {
        history_.pop_front();
    }
    const sim::MatchState& perceived = history_.front();  // мир reactionFrames тиков назад

    const sim::FighterState& me = perceived.fighter(self_);
    const sim::FighterState& them = perceived.fighter(sim::opponentOf(self_));
    const std::int32_t distance = std::abs(them.position.x - me.position.x);

    // TODO(§6): utility scoring намерений (сблизиться, тычок, наказать, анти-эйр, блок, снаряд, комбо)
    // и исполнение «скриптами ввода» — последовательностями InputFrame для команд вроде "B,F+2".
    sim::InputFrame input;
    if (distance > profile_.preferredDistance) {
        const int towards = them.position.x > me.position.x ? 1 : -1;  // направление на экране
        input.direction = sim::makeDirection(towards, 0);
    } else if (rng_.nextBelow(100) < 2) {
        input.direction = sim::makeDirection(0, 1);  // изредка прыгает, чтобы был виден прыжок
    }
    return input;
}

}  // namespace dc::ai
