#include "presentation/fighter_view.h"

#include <cstddef>

namespace dc::presentation {

std::string_view FighterView::clipFor(const sim::FighterState& fighter, const sim::CharacterData& character) noexcept {
    switch (fighter.action) {
    case sim::Action::Idle: return "idle";
    case sim::Action::WalkForward: return "walk_f";
    case sim::Action::WalkBack: return "walk_b";
    case sim::Action::Crouch: return "crouch";
    case sim::Action::JumpSquat: return "jump_start";
    case sim::Action::Airborne: return "jump";
    case sim::Action::Landing: return "land";
    case sim::Action::Attack: {
        const auto index = static_cast<std::size_t>(fighter.move);
        if (fighter.move != sim::MoveId::None && index < character.moves.size()) {
            return character.moves[index].animation;
        }
        return "idle";
    }
    case sim::Action::Blockstun: return "block_hit";
    case sim::Action::Hitstun: return "hit_high_light";
    case sim::Action::Juggle: return "juggle";
    case sim::Action::Knockdown: return "knockdown";
    case sim::Action::Wakeup: return "getup";
    case sim::Action::Throwing: return "throw";
    case sim::Action::Thrown: return "thrown";
    case sim::Action::Dizzy: return "dizzy";
    case sim::Action::Cinematic: return "cinematic";
    case sim::Action::Victory: return "victory_1";
    case sim::Action::Defeated: return "defeated";
    }
    return "idle";
}

void FighterView::update(const sim::FighterState& fighter, const sim::CharacterData& character) noexcept {
    currentClip_ = clipFor(fighter, character);
}

}  // namespace dc::presentation
