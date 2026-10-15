#pragma once

#include "sim/match_data.h"
#include "sim/simulation.h"

#include <cstdint>

namespace dc::test {

// Данные матча прямо в коде: тесты симуляции не зависят от JSON на диске.
inline sim::MatchData makeTestMatchData() {
    sim::CharacterData fighter;
    fighter.id = "test";
    fighter.pushboxStanding = {{-250, 0}, {250, 1750}};
    fighter.pushboxCrouching = {{-250, 0}, {250, 1150}};
    fighter.pushboxAirborne = {{-220, 300}, {220, 1600}};

    sim::MatchData data;
    data.characters = {fighter, fighter};
    return data;
}

inline sim::StepInput holdDirections(std::uint8_t p1, std::uint8_t p2) {
    sim::StepInput input;
    input.players[0].direction = p1;
    input.players[1].direction = p2;
    return input;
}

// Прокручивает интро раунда, пока не начнётся бой.
inline void skipIntro(sim::MatchState& state, const sim::MatchData& data) {
    sim::EventList events;
    for (std::int32_t i = 0; i < data.rules.introFrames; ++i) {
        events.clear();
        sim::step(state, data, holdDirections(5, 5), events);
    }
}

}  // namespace dc::test
