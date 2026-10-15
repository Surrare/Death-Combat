#pragma once

#include <cstddef>
#include <filesystem>

namespace dc::assets {

// Краткая сводка .glb — первая проверка пайплайна Blender → glTF (docs/ARCHITECTURE.md §9).
struct GltfSummary {
    std::size_t meshes{0};
    std::size_t skins{0};
    std::size_t animations{0};
    std::size_t materials{0};
};

[[nodiscard]] GltfSummary inspectGltf(const std::filesystem::path& path);

// TODO(§9): loadModel(path) → меши, скелет, клипы, материалы; маркеры клипов из anims.json рядом с .glb.

}  // namespace dc::assets
