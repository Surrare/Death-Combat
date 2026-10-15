#include "data/game_data.h"

#include "assets/json.h"
#include "core/errors.h"
#include "core/file_io.h"

#include <array>
#include <cstdint>
#include <string>

namespace dc::data {
namespace {

using assets::readField;
using assets::readFieldOr;

sim::Box readBox(const nlohmann::json& object, const char* key, std::string_view where) {
    const auto values = readField<std::array<std::int32_t, 4>>(object, key, where);  // [minX, minY, maxX, maxY]
    if (values[0] >= values[2] || values[1] >= values[3]) {
        throw core::DataError{std::string{where} + "." + key + ": min должен быть меньше max"};
    }
    return sim::Box{{values[0], values[1]}, {values[2], values[3]}};
}

sim::HitStrength parseStrength(const std::string& text, std::string_view where) {
    if (text == "light") return sim::HitStrength::Light;
    if (text == "medium") return sim::HitStrength::Medium;
    if (text == "heavy") return sim::HitStrength::Heavy;
    if (text == "special") return sim::HitStrength::Special;
    throw core::DataError{std::string{where} + ".strength: неизвестное значение \"" + text + "\""};
}

sim::AttackHeight parseHeight(const std::string& text, std::string_view where) {
    if (text == "high") return sim::AttackHeight::High;
    if (text == "mid") return sim::AttackHeight::Mid;
    if (text == "low") return sim::AttackHeight::Low;
    if (text == "overhead") return sim::AttackHeight::Overhead;
    throw core::DataError{std::string{where} + ".height: неизвестное значение \"" + text + "\""};
}

sim::MoveData readMove(const nlohmann::json& json, std::string_view file) {
    sim::MoveData move;
    move.id = readField<std::string>(json, "id", file);
    const std::string where = std::string{file} + ": " + move.id;

    move.command = readField<std::string>(json, "command", where);
    move.animation = readFieldOr<std::string>(json, "animation", move.id, where);
    move.startup = readField<std::int32_t>(json, "startup", where);
    move.active = readFieldOr<std::int32_t>(json, "active", 0, where);
    move.recovery = readFieldOr<std::int32_t>(json, "recovery", 0, where);
    move.damage = readFieldOr<std::int32_t>(json, "damage", 0, where);
    move.chip = readFieldOr<std::int32_t>(json, "chip", 0, where);
    move.strength = parseStrength(readFieldOr<std::string>(json, "strength", "light", where), where);
    move.height = parseHeight(readFieldOr<std::string>(json, "height", "high", where), where);
    move.hitstun = readFieldOr<std::int32_t>(json, "hitstun", 0, where);
    move.blockstun = readFieldOr<std::int32_t>(json, "blockstun", 0, where);
    move.hitstop = readFieldOr<std::int32_t>(json, "hitstop", 0, where);

    if (move.startup < 1 || move.active < 0 || move.recovery < 0) {
        throw core::DataError{where + ": startup >= 1, active >= 0, recovery >= 0"};
    }

    const std::int32_t firstActive = move.startup;
    const std::int32_t lastActive = move.startup + move.active - 1;
    for (const nlohmann::json& hitbox : json.value("hitboxes", nlohmann::json::array())) {
        const auto frames = readField<std::array<std::int32_t, 2>>(hitbox, "frames", where);
        if (frames[0] < firstActive || frames[1] > lastActive || frames[0] > frames[1]) {
            throw core::DataError{where + ".hitboxes: кадры [" + std::to_string(frames[0]) + ", " +
                                  std::to_string(frames[1]) + "] вне активных кадров приёма"};
        }
        move.hitboxes.push_back({frames[0], frames[1], readBox(hitbox, "box", where)});
    }
    // TODO(§4.5): cancels, effects, hurtboxOverrides, events.
    return move;
}

}  // namespace

sim::Rules loadRules(const std::filesystem::path& file) {
    const nlohmann::json json = assets::loadJson(file);
    const std::string where = core::pathToUtf8(file.filename());
    sim::Rules rules;
    rules.roundSeconds = readField<std::int32_t>(json, "roundSeconds", where);
    rules.roundsToWin = readField<std::int32_t>(json, "roundsToWin", where);
    rules.introFrames = readFieldOr(json, "introFrames", rules.introFrames, where);
    rules.roundOverFrames = readFieldOr(json, "roundOverFrames", rules.roundOverFrames, where);
    rules.finishHimFrames = readFieldOr(json, "finishHimFrames", rules.finishHimFrames, where);
    rules.arenaHalfWidth = readFieldOr(json, "arenaHalfWidth", rules.arenaHalfWidth, where);
    rules.maxFighterDistance = readFieldOr(json, "maxFighterDistance", rules.maxFighterDistance, where);
    rules.startDistance = readFieldOr(json, "startDistance", rules.startDistance, where);
    rules.fatalBlowHealthThreshold = readFieldOr(json, "fatalBlowHealthThreshold", rules.fatalBlowHealthThreshold, where);
    return rules;
}

sim::CharacterData loadCharacter(const std::filesystem::path& characterDirectory) {
    const std::filesystem::path characterFile = characterDirectory / "character.json";
    const nlohmann::json json = assets::loadJson(characterFile);
    const std::string where = core::pathToUtf8(characterDirectory.filename() / "character.json");

    sim::CharacterData character;
    character.id = readField<std::string>(json, "id", where);
    character.displayName = readFieldOr<std::string>(json, "displayName", character.id, where);
    character.model = readFieldOr<std::string>(json, "model", "", where);

    const auto stats = readField<nlohmann::json>(json, "stats", where);
    sim::CharacterStats& s = character.stats;
    s.health = readField<std::int32_t>(stats, "health", where);
    s.walkForwardSpeed = readField<std::int32_t>(stats, "walkForwardSpeed", where);
    s.walkBackSpeed = readField<std::int32_t>(stats, "walkBackSpeed", where);
    s.jumpVelocity = readField<std::int32_t>(stats, "jumpVelocity", where);
    s.jumpHorizontalSpeed = readField<std::int32_t>(stats, "jumpHorizontalSpeed", where);
    s.gravity = readField<std::int32_t>(stats, "gravity", where);
    s.jumpSquatFrames = readFieldOr(stats, "jumpSquatFrames", s.jumpSquatFrames, where);
    s.landingFrames = readFieldOr(stats, "landingFrames", s.landingFrames, where);

    const auto pushbox = readField<nlohmann::json>(json, "pushbox", where);
    character.pushboxStanding = readBox(pushbox, "standing", where);
    character.pushboxCrouching = readBox(pushbox, "crouching", where);
    character.pushboxAirborne = readBox(pushbox, "airborne", where);

    const std::filesystem::path movesFile = characterDirectory / "moves.json";
    const nlohmann::json moves = assets::loadJson(movesFile);
    const std::string movesWhere = core::pathToUtf8(characterDirectory.filename() / "moves.json");
    for (const nlohmann::json& move : readField<nlohmann::json>(moves, "moves", movesWhere)) {
        character.moves.push_back(readMove(move, movesWhere));
    }
    return character;
}

sim::MatchData loadMatchData(const std::filesystem::path& assetsRoot, std::string_view fighterP1,
                             std::string_view fighterP2) {
    sim::MatchData data;
    data.rules = loadRules(assetsRoot / "rules.json");
    data.characters[0] = loadCharacter(assetsRoot / "characters" / std::filesystem::path{fighterP1});
    data.characters[1] = loadCharacter(assetsRoot / "characters" / std::filesystem::path{fighterP2});
    return data;
}

}  // namespace dc::data
