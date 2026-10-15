#pragma once

#include "sim/types.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dc::sim {

// Неизменяемые данные матча: читаются из JSON один раз (game/data) и не входят в MatchState,
// поэтому здесь можно std::string и std::vector.

struct HitboxData {
    std::int32_t firstFrame{1};  // кадры приёма считаются с 1, как в frame data
    std::int32_t lastFrame{1};
    Box box;
};

// Один приём из moves.json (docs/ARCHITECTURE.md §4.5).
struct MoveData {
    std::string id;
    std::string command;  // нотация MK: "1", "B,F+2", "D+4"
    std::string animation;
    std::int32_t startup{1};  // кадры до первого активного, включая его
    std::int32_t active{1};
    std::int32_t recovery{0};
    std::int32_t damage{0};
    std::int32_t chip{0};
    HitStrength strength{HitStrength::Light};
    AttackHeight height{AttackHeight::High};
    std::int32_t hitstun{0};
    std::int32_t blockstun{0};
    std::int32_t hitstop{0};
    std::vector<HitboxData> hitboxes;
    // TODO(§4.5): cancels, effects (spawnProjectile, teleport, armor…), hurtboxOverrides, events.

    // Полная длина приёма: startup уже включает первый активный кадр.
    [[nodiscard]] std::int32_t totalFrames() const noexcept { return startup + active + recovery - 1; }
};

struct CharacterStats {
    std::int32_t health{1000};
    std::int32_t walkForwardSpeed{32};  // мм за тик
    std::int32_t walkBackSpeed{26};
    std::int32_t jumpVelocity{160};
    std::int32_t jumpHorizontalSpeed{45};
    std::int32_t gravity{8};  // мм за тик²
    std::int32_t jumpSquatFrames{4};
    std::int32_t landingFrames{3};
};

struct CharacterData {
    std::string id;
    std::string displayName;
    std::string model;  // путь к .glb относительно assets/
    CharacterStats stats;
    Box pushboxStanding;
    Box pushboxCrouching;
    Box pushboxAirborne;
    std::vector<MoveData> moves;

    [[nodiscard]] const MoveData* findMove(std::string_view moveId) const noexcept;
};

// Общие правила матча из rules.json.
struct Rules {
    std::int32_t roundSeconds{90};
    std::int32_t roundsToWin{2};
    std::int32_t introFrames{120};
    std::int32_t roundOverFrames{180};
    std::int32_t finishHimFrames{480};
    std::int32_t arenaHalfWidth{6000};      // мм от центра арены до стены
    std::int32_t maxFighterDistance{5000};  // бойцы не расходятся шире кадра камеры
    std::int32_t startDistance{2400};
    std::int32_t fatalBlowHealthThreshold{300};
    // TODO(§4.7): скейлинг комбо, хитстоп по силе удара, окна ввода (§5).
};

struct MatchData {
    std::array<CharacterData, 2> characters;
    Rules rules;
};

}  // namespace dc::sim
