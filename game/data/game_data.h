#pragma once

#include "sim/match_data.h"

#include <filesystem>
#include <string_view>

namespace dc::data {

// Загрузка данных симуляции из JSON (docs/ARCHITECTURE.md §4.5). При ошибке — core::DataError
// с файлом и полем, например «moves.json: jab: нет поля "startup"».

[[nodiscard]] sim::Rules loadRules(const std::filesystem::path& file);

// characterDirectory/character.json + characterDirectory/moves.json
[[nodiscard]] sim::CharacterData loadCharacter(const std::filesystem::path& characterDirectory);

// assets/rules.json + assets/characters/<id>/ для обоих бойцов.
[[nodiscard]] sim::MatchData loadMatchData(const std::filesystem::path& assetsRoot, std::string_view fighterP1,
                                           std::string_view fighterP2);

}  // namespace dc::data
