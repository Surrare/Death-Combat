#include "core/pcg32.h"
#include "core/static_vector.h"
#include "sim/collision.h"
#include "sim/command_interpreter.h"
#include "sim/state_hash.h"
#include "test_helpers.h"

#include <doctest/doctest.h>

#include <cstring>
#include <type_traits>
#include <variant>

using namespace dc;

TEST_CASE("StaticVector: push, removeAt, переполнение") {
    core::StaticVector<int, 3> values;
    CHECK(values.push(1));
    CHECK(values.push(2));
    CHECK(values.push(3));
    CHECK_FALSE(values.push(4));
    values.removeAt(0);
    CHECK(values.size() == 2);
    CHECK(values[0] == 3);
}

TEST_CASE("Pcg32: одинаковое зерно — одинаковая последовательность") {
    core::Pcg32 a{42};
    core::Pcg32 b{42};
    for (int i = 0; i < 1000; ++i) {
        CHECK(a.next() == b.next());
    }
    core::Pcg32 c{7};
    for (int i = 0; i < 1000; ++i) {
        CHECK(c.nextBelow(10) < 10u);
    }
}

TEST_CASE("Коллизии: бокс отражается по стороне взгляда") {
    const sim::Box local{{100, 0}, {300, 200}};
    const sim::Box right = sim::toWorld(local, {1000, 0}, 1);
    const sim::Box left = sim::toWorld(local, {1000, 0}, -1);
    CHECK(right == sim::Box{{1100, 0}, {1300, 200}});
    CHECK(left == sim::Box{{700, 0}, {900, 200}});
    CHECK(sim::overlaps(right, sim::Box{{1200, 100}, {1250, 150}}));
    CHECK_FALSE(sim::overlaps(right, left));
}

TEST_CASE("Направление относительно бойца зеркалится при facing = -1") {
    CHECK(sim::toRelativeDirection(6, 1) == 6);
    CHECK(sim::toRelativeDirection(6, -1) == 4);
    CHECK(sim::toRelativeDirection(3, -1) == 1);
    CHECK(sim::toRelativeDirection(9, -1) == 7);
    CHECK(sim::toRelativeDirection(5, -1) == 5);
}

TEST_CASE("MatchState копируется memcpy без потерь") {
    static_assert(std::is_trivially_copyable_v<sim::MatchState>);
    const sim::MatchData data = test::makeTestMatchData();
    const sim::MatchState original = sim::makeInitialState(data, 1);
    sim::MatchState copy;
    std::memcpy(&copy, &original, sizeof(sim::MatchState));
    CHECK(sim::hashState(copy) == sim::hashState(original));
}

TEST_CASE("Во время интро бойцы не двигаются") {
    const sim::MatchData data = test::makeTestMatchData();
    sim::MatchState state = sim::makeInitialState(data, 1);
    const std::int32_t startX = state.fighters[0].position.x;
    sim::EventList events;
    sim::step(state, data, test::holdDirections(6, 5), events);
    CHECK(state.fighters[0].position.x == startX);
    CHECK(std::holds_alternative<sim::RoundStarted>(events[0]));
}

TEST_CASE("Ходьба вперёд: ровно walkForwardSpeed мм за тик") {
    const sim::MatchData data = test::makeTestMatchData();
    sim::MatchState state = sim::makeInitialState(data, 1);
    test::skipIntro(state, data);
    REQUIRE(state.round.phase == sim::RoundPhase::Fight);

    const std::int32_t startX = state.fighters[0].position.x;
    sim::EventList events;
    for (int i = 0; i < 10; ++i) {
        events.clear();
        sim::step(state, data, test::holdDirections(6, 5), events);
    }
    CHECK(state.fighters[0].action == sim::Action::WalkForward);
    CHECK(state.fighters[0].position.x == startX + 10 * data.characters[0].stats.walkForwardSpeed);
}

TEST_CASE("Пушбоксы не дают пройти сквозь соперника") {
    const sim::MatchData data = test::makeTestMatchData();
    sim::MatchState state = sim::makeInitialState(data, 1);
    test::skipIntro(state, data);

    sim::EventList events;
    for (int i = 0; i < 300; ++i) {
        events.clear();
        sim::step(state, data, test::holdDirections(6, 4), events);  // оба идут навстречу
    }
    const std::int32_t gap = state.fighters[1].position.x - state.fighters[0].position.x;
    CHECK(gap >= 500 - 1);  // две полуширины пушбокса (±1 мм на округление)
}

TEST_CASE("Прыжок: взлёт после jumpSquat, приземление с событием Landed") {
    const sim::MatchData data = test::makeTestMatchData();
    sim::MatchState state = sim::makeInitialState(data, 1);
    test::skipIntro(state, data);

    sim::EventList events;
    sim::step(state, data, test::holdDirections(8, 5), events);
    CHECK(state.fighters[0].action == sim::Action::JumpSquat);

    bool landed = false;
    for (int i = 0; i < 120 && !landed; ++i) {
        events.clear();
        sim::step(state, data, test::holdDirections(5, 5), events);
        for (const sim::GameEvent& event : events) {
            landed = landed || std::holds_alternative<sim::Landed>(event);
        }
    }
    CHECK(landed);
    CHECK(state.fighters[0].position.y == 0);
}

TEST_CASE("Детерминизм: одинаковый ввод даёт одинаковый хеш на каждом тике") {
    const sim::MatchData data = test::makeTestMatchData();
    sim::MatchState a = sim::makeInitialState(data, 2026);
    sim::MatchState b = sim::makeInitialState(data, 2026);
    core::Pcg32 inputs{99};

    sim::EventList eventsA;
    sim::EventList eventsB;
    for (int tick = 0; tick < 5000; ++tick) {
        sim::StepInput input;
        for (sim::InputFrame& player : input.players) {
            player.direction = static_cast<std::uint8_t>(1 + inputs.nextBelow(9));
            player.buttons = static_cast<std::uint8_t>(inputs.nextBelow(64));
        }
        eventsA.clear();
        eventsB.clear();
        sim::step(a, data, input, eventsA);
        sim::step(b, data, input, eventsB);
        REQUIRE(sim::hashState(a) == sim::hashState(b));
    }
}

TEST_CASE("Время раунда вышло — RoundEnded с timeout") {
    sim::MatchData data = test::makeTestMatchData();
    data.rules.roundSeconds = 1;
    sim::MatchState state = sim::makeInitialState(data, 1);
    test::skipIntro(state, data);

    bool ended = false;
    sim::EventList events;
    for (int i = 0; i < sim::ticksPerSecond + 1 && !ended; ++i) {
        events.clear();
        sim::step(state, data, test::holdDirections(5, 5), events);
        for (const sim::GameEvent& event : events) {
            if (const auto* roundEnded = std::get_if<sim::RoundEnded>(&event)) {
                ended = roundEnded->timeout;
            }
        }
    }
    CHECK(ended);
    CHECK(state.round.phase == sim::RoundPhase::Over);
}
