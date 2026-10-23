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
                    if (std::fabs(f.pos.x) > FightSim::STAGE_HALF + 0.01f) { std::printf("out of stage: %d vs %d x=%f\n", a, b, f.pos.x); failures++; }
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
