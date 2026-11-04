// Headless simulation smoke test: AI vs AI matches with no window or OpenGL.
// Verifies that matches finish, mechanics fire, and nothing produces NaN or stuck states.
#include "game/fight.h"
#include "game/fight_ai.h"
#include "game/characters.h"
#include <cstdio>
#include <cmath>
#include <map>

using namespace dc;

int main() {
    const auto& ros = roster();
    int failures = 0;
    auto simpleMoveWorks = [&](const Character& character, Motion motion, float moveF, int dirY,
                               uint32_t button, MoveKind kind, float startingHeat) {
        FightSim sim;
        sim.init(&character, &ros[(static_cast<size_t>(&character - ros.data()) + 1) % ros.size()], 2);
        sim.fighter(0).hhg = startingHeat;
        SimInput neutral;
        for (int i = 0; i < 150; ++i) sim.step(neutral, neutral);
        SimInput input;
        input.moveF = moveF;
        input.dirY = dirY;
        input.pressed = button;
        sim.step(input, neutral);
        input.pressed = 0;
        sim.step(input, neutral);
        const Move* move = sim.fighter(0).move;
        return move && move->kind == kind && move->motion == motion;
    };
    for (const Character& character : ros) {
        if (character.hp != Character::BASE_HP) {
            std::printf("Unequal HP for %s: %.0f\n", character.name.c_str(), character.hp);
            failures++;
        }
        if (!simpleMoveWorks(character, Motion::QCF, 1, 0, BTN_LIGHT, MoveKind::Special, 0)) {
            std::printf("Forward + Light failed for %s\n", character.name.c_str());
            failures++;
        }
        if (!simpleMoveWorks(character, Motion::QCB, -1, 0, BTN_LIGHT, MoveKind::Special, 0)) {
            std::printf("Back + Light failed for %s\n", character.name.c_str());
            failures++;
        }
        if (!simpleMoveWorks(character, Motion::DP, 1, 0, BTN_HEAVY, MoveKind::Special, 0)) {
            std::printf("Forward + Heavy failed for %s\n", character.name.c_str());
            failures++;
        }
        if (!simpleMoveWorks(character, Motion::QCF, 1, 0, BTN_LIGHT | BTN_MEDIUM, MoveKind::HHA, 1000)) {
            std::printf("Forward + Light/Medium HHA failed for %s\n", character.name.c_str());
            failures++;
        }
        if (!simpleMoveWorks(character, Motion::QCF, 1, 0, BTN_LIGHT | BTN_MEDIUM | BTN_HEAVY, MoveKind::GHA, 2000)) {
            std::printf("Forward + Light/Medium/Heavy GHA failed for %s\n", character.name.c_str());
            failures++;
        }
    }

    // ---- free movement on the arena floor
    {
        FightSim sim;
        sim.init(&ros[0], &ros[1], 2);
        SimInput neutral;
        for (int i = 0; i < 150; ++i) sim.step(neutral, neutral);
        const float startGap = distance(sim.fighter(0).pos, sim.fighter(1).pos);
        SimInput circle; circle.moveS = 1;
        for (int i = 0; i < 120; ++i) sim.step(circle, neutral);
        const Fighter& f = sim.fighter(0); const Fighter& o = sim.fighter(1);
        Vec3 to = o.pos - f.pos; to.y = 0;
        if (std::fabs(f.pos.z) < 1.0f) { std::printf("circling did not leave the start line (z=%f)\n", f.pos.z); failures++; }
        if (dot(normalize(to), f.fwd) < 0.98f) { std::printf("fighter does not stay locked on while circling\n"); failures++; }
        if (std::fabs(to.length() - startGap) > 1.2f) { std::printf("circling drifted: gap %f -> %f\n", startGap, to.length()); failures++; }

        // dash + sideways = sidestep: lateral hop with strike invulnerability
        for (int i = 0; i < 30; ++i) sim.step(neutral, neutral);
        Vec3 before = sim.fighter(0).pos; Vec3 right = sim.fighter(0).right();
        SimInput step; step.moveS = 1; step.pressed = BTN_DASH;
        sim.step(step, neutral);
        bool invul = sim.fighter(0).invulStrikes > 0 && sim.fighter(0).state == FState::Sidestep;
        for (int i = 0; i < 25; ++i) sim.step(neutral, neutral);
        float lateral = dot(sim.fighter(0).pos - before, right);
        if (!invul || lateral < 0.6f) { std::printf("sidestep failed (invul %d, lateral %f)\n", (int)invul, lateral); failures++; }

        // walking into the edge keeps the fighter inside the arena
        SimInput back; back.moveF = -1;
        for (int i = 0; i < 600; ++i) sim.step(back, neutral);
        if (!sim.insideArena(sim.fighter(0).pos, 0.001f)) { std::printf("walked out of the arena\n"); failures++; }
    }

    std::map<std::string, int> eventCounts;
    int totalHits = 0, matches = 0, projectilesSeen = 0, maxCombo = 0, clashes = 0;
    for (int a = 0; a < (int)ros.size(); ++a)
        for (int b = 0; b < (int)ros.size(); ++b) {
            if (a == b) continue;
            FightSim sim;
            sim.init(&ros[a], &ros[b], 2);
            FightAI ai[2];
            ai[0].reset(2, 100 + a * 7 + b); ai[1].reset(2, 200 + b * 11 + a);
            int frames = 0;
            while (sim.phase() != Phase::MatchOver && frames < 60 * 60 * 8) {
                SimInput ia = ai[0].think(sim, 0), ib = ai[1].think(sim, 1);
                sim.step(ia, ib);
                for (const FightEvent& e : sim.events()) {
                    eventCounts[std::to_string((int)e.type)]++;
                    if (e.type == FightEvent::Hit) totalHits++;
                    if (e.type == FightEvent::Clash) clashes++;
                }
                projectilesSeen += (int)sim.projectiles().size();
                for (int i = 0; i < 2; ++i) {
                    const Fighter& f = sim.fighter(i);
                    maxCombo = std::max(maxCombo, f.comboHits);
                    if (std::isnan(f.pos.x) || std::isnan(f.pos.y) || std::isnan(f.hp)) { std::printf("NaN in match %d vs %d\n", a, b); failures++; break; }
                    if (!sim.insideArena(f.pos, 0.01f)) { std::printf("out of arena: %d vs %d (%f, %f)\n", a, b, f.pos.x, f.pos.z); failures++; }
                    if (std::isnan(f.fwd.x) || std::fabs(f.fwd.length() - 1.0f) > 0.01f) { std::printf("bad facing in %d vs %d\n", a, b); failures++; }
                    if (f.pos.y < -0.01f) { std::printf("below floor\n"); failures++; }
                }
                frames++;
            }
            if (sim.phase() != Phase::MatchOver) { std::printf("match %s vs %s did not finish in time\n", ros[a].name.c_str(), ros[b].name.c_str()); failures++; }
            matches++;
            std::printf("%-7s vs %-7s  rounds %d  winner %s  frames %d\n", ros[a].name.c_str(), ros[b].name.c_str(), sim.round(),
                        sim.matchWinner() >= 0 ? sim.fighter(sim.matchWinner()).ch->name.c_str() : "none", frames);
        }
    std::printf("\nmatches %d, hits %d, max combo %d, clashes %d, projectile-frames %d\n", matches, totalHits, maxCombo, clashes, projectilesSeen);
    const char* names[] = {"Hit", "Block", "Throw", "Spark", "Text", "Projectile", "Clash", "StyleBreak", "FlashCancel", "KO", "RoundStart", "MoveName", "Dust", "Land", "Tech"};
    for (int i = 0; i < 15; ++i) std::printf("  %-12s %d\n", names[i], eventCounts[std::to_string(i)]);
    if (totalHits == 0) { std::printf("no hits at all!\n"); failures++; }
    std::printf(failures ? "\nFAILED (%d)\n" : "\nOK\n", failures);
    return failures ? 1 : 0;
}
