#pragma once

#include "core/errors.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace dc::assets {

// JSON-файл целиком. Синтаксическая ошибка → DataError с именем файла и позицией.
[[nodiscard]] nlohmann::json loadJson(const std::filesystem::path& path);

// Обязательное поле. Вместо голого исключения библиотеки — DataError вида
// «moves.json: jab: нет поля "startup"» (docs/ARCHITECTURE.md §4.5).
template <typename T>
[[nodiscard]] T readField(const nlohmann::json& object, const char* key, std::string_view where) {
    const auto it = object.find(key);
    if (it == object.end()) {
        throw core::DataError{std::string{where} + ": нет поля \"" + key + "\""};
    }
    try {
        return it->template get<T>();
    } catch (const nlohmann::json::exception& error) {
        throw core::DataError{std::string{where} + "." + key + ": " + error.what()};
    }
}

// Необязательное поле со значением по умолчанию.
template <typename T>
[[nodiscard]] T readFieldOr(const nlohmann::json& object, const char* key, const T& fallback, std::string_view where) {
    return object.contains(key) ? readField<T>(object, key, where) : fallback;
}

}  // namespace dc::assets
