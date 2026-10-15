#include "ai/ai_controller.h"
#include "controls/bindings.h"
#include "test_helpers.h"

#include <doctest/doctest.h>

using namespace dc;

TEST_CASE("SOCD: противоположные направления дают нейтраль") {
    CHECK(controls::directionFromKeys(false, false, true, true) == 5);
    CHECK(controls::directionFromKeys(true, true, false, false) == 5);
    CHECK(controls::directionFromKeys(true, false, false, true) == 9);
    CHECK(controls::directionFromKeys(false, true, true, false) == 1);
    CHECK(controls::directionFromKeys(false, false, false, false) == 5);
}

TEST_CASE("ИИ видит мир с задержкой и идёт к сопернику") {
    sim::MatchData data = test::makeTestMatchData();
    data.rules.startDistance = 4000;  // дальше preferredDistance любого уровня
    const sim::MatchState state = sim::makeInitialState(data, 1);

    ai::AiController ai{sim::PlayerIndex::P2, ai::AiProfile::forDifficulty(ai::AiDifficulty::Hard), 5};
    sim::InputFrame input;
    for (int i = 0; i < 20; ++i) {
        input = ai.think(state);
    }
    CHECK(input.direction == 4);  // P2 справа, соперник слева
}
