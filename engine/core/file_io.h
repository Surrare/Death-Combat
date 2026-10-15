#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace dc::core {

// Читают файл целиком; если не получилось — AssetError с путём.
[[nodiscard]] std::string readTextFile(const std::filesystem::path& path);
[[nodiscard]] std::vector<std::byte> readBinaryFile(const std::filesystem::path& path);

// Путь в UTF-8 для логов и сообщений (path::string() на Windows портит кириллицу).
[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& path);

// Папка с .exe.
[[nodiscard]] std::filesystem::path executableDirectory();

// Где лежат ассеты. Debug: прямо assets/ из исходников (правки видны без пересборки).
// Release: assets/ рядом с .exe (туда их копирует сборка).
[[nodiscard]] std::filesystem::path assetsDirectory();

}  // namespace dc::core
