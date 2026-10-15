// Единственное место, где компилируются реализации header-only библиотек.
#include "assets/gltf.h"
#include "assets/image.h"

#include "core/errors.h"
#include "core/file_io.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <memory>

namespace dc::assets {

GltfSummary inspectGltf(const std::filesystem::path& path) {
    const std::string utf8Path = core::pathToUtf8(path);
    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, utf8Path.c_str(), &data) != cgltf_result_success) {
        throw core::AssetError{"Не удалось разобрать glTF: " + utf8Path};
    }
    const std::unique_ptr<cgltf_data, decltype(&cgltf_free)> guard{data, &cgltf_free};
    return GltfSummary{data->meshes_count, data->skins_count, data->animations_count, data->materials_count};
}

ImageData loadImageRgba8(const std::filesystem::path& path) {
    const std::vector<std::byte> file = core::readBinaryFile(path);
    int width{0};
    int height{0};
    int channels{0};
    stbi_uc* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(file.data()),
                                            static_cast<int>(file.size()), &width, &height, &channels, 4);
    if (pixels == nullptr) {
        throw core::AssetError{"Не удалось декодировать изображение " + core::pathToUtf8(path) + ": " +
                               stbi_failure_reason()};
    }
    const std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> guard{pixels, &stbi_image_free};

    ImageData image;
    image.width = width;
    image.height = height;
    image.pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    return image;
}

}  // namespace dc::assets
