#include "core/file_io.h"

#include "core/errors.h"

#include <windows.h>

#include <fstream>
#include <iterator>

namespace dc::core {
namespace {

std::ifstream openOrThrow(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw AssetError{"Не удалось открыть файл: " + pathToUtf8(path)};
    }
    return file;
}

}  // namespace

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file = openOrThrow(path);
    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

std::vector<std::byte> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream file = openOrThrow(path);
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

std::string pathToUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string{text.begin(), text.end()};
}

std::filesystem::path executableDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw InitError{"GetModuleFileNameW не вернул путь к .exe"};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path{buffer}.parent_path();
}

std::filesystem::path assetsDirectory() {
#if !defined(NDEBUG) && defined(DC_SOURCE_ASSETS_DIR)
    const std::filesystem::path sourceAssets{u8"" DC_SOURCE_ASSETS_DIR};
    if (std::filesystem::exists(sourceAssets)) {
        return sourceAssets;
    }
#endif
    return executableDirectory() / "assets";
}

}  // namespace dc::core
