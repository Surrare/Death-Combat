#include "sim/debug_names.h"

namespace dc::sim {

const char* toString(Action action) noexcept {
    switch (action) {
    case Action::Idle: return "Idle";
    case Action::WalkForward: return "WalkForward";
    case Action::WalkBack: return "WalkBack";
    case Action::Crouch: return "Crouch";
    case Action::JumpSquat: return "JumpSquat";
    case Action::Airborne: return "Airborne";
    case Action::Landing: return "Landing";
    case Action::Attack: return "Attack";
    case Action::Blockstun: return "Blockstun";
    case Action::Hitstun: return "Hitstun";
    case Action::Juggle: return "Juggle";
    case Action::Knockdown: return "Knockdown";
    case Action::Wakeup: return "Wakeup";
    case Action::Throwing: return "Throwing";
    case Action::Thrown: return "Thrown";
    case Action::Dizzy: return "Dizzy";
    case Action::Cinematic: return "Cinematic";
    case Action::Victory: return "Victory";
    case Action::Defeated: return "Defeated";
    }
    return "?";
}

const char* toString(RoundPhase phase) noexcept {
    switch (phase) {
    case RoundPhase::Intro: return "Intro";
    case RoundPhase::Fight: return "Fight";
    case RoundPhase::FinishHim: return "FinishHim";
    case RoundPhase::Over: return "Over";
    }
    return "?";
}

}  // namespace dc::sim
