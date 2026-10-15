#include "anim/animation_clip.h"
#include "core/errors.h"
#include "data/game_data.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace dc;

TEST_CASE("Данные из assets/ загружаются и проходят проверки") {
    const std::filesystem::path assets{u8"" DC_TEST_ASSETS_DIR};
    const sim::MatchData data = data::loadMatchData(assets, "fighter_red", "fighter_blue");
    CHECK(data.rules.roundsToWin == 2);
    CHECK(data.characters[0].id == "fighter_red");
    REQUIRE(data.characters[0].findMove("jab") != nullptr);
    CHECK(data.characters[0].findMove("jab")->totalFrames() == 20);
}

TEST_CASE("Ошибка в JSON — понятная DataError, а не падение") {
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "dc_tests_bad_character";
    std::filesystem::create_directories(directory);
    std::ofstream{directory / "character.json"} << R"({ "id": "bad", "stats": {} })";
    CHECK_THROWS_AS(static_cast<void>(data::loadCharacter(directory)), core::DataError);
    std::filesystem::remove_all(directory);
}

TEST_CASE("Time-warp анимации: удар совпадает с первым активным кадром") {
    constexpr float impact = 0.30f;
    constexpr float duration = 0.80f;
    CHECK(anim::warpMoveTime(0, 7, 20, impact, duration) == doctest::Approx(0.0f));
    CHECK(anim::warpMoveTime(7, 7, 20, impact, duration) == doctest::Approx(impact));
    CHECK(anim::warpMoveTime(20, 7, 20, impact, duration) == doctest::Approx(duration));
}
