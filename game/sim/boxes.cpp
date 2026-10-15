#include "sim/boxes.h"

namespace dc::sim {

const Box& currentPushbox(const FighterState& fighter, const CharacterData& character) noexcept {
    switch (fighter.action) {
    case Action::Crouch:
        return character.pushboxCrouching;
    case Action::Airborne:
    case Action::Juggle:
        return character.pushboxAirborne;
    default:
        return character.pushboxStanding;
    }
}

}  // namespace dc::sim
