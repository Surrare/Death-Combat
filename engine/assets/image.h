#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace dc::assets {

// Декодированное изображение RGBA8 (stb_image). Декодирование можно делать в рабочем потоке,
// загрузку в GPU — только в главном (docs/ARCHITECTURE.md §9).
struct ImageData {
    int width{0};
    int height{0};
    std::vector<std::uint8_t> pixels;  // width * height * 4
};

[[nodiscard]] ImageData loadImageRgba8(const std::filesystem::path& path);

}  // namespace dc::assets
