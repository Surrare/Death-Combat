#pragma once

#include <filesystem>
#include <string_view>

namespace dc::assets {

// Время жизни ресурса (docs/ARCHITECTURE.md §9): Global — шрифты, UI, общие звуки;
// Match — бойцы и арена, выгружаются при выходе из матча.
enum class AssetScope { Global, Match };

// Хранилище ресурсов. Сейчас только переводит относительные пути в абсолютные.
// TODO(§9): кеш по пути с core::Handle<T>, области жизни, асинхронная загрузка в пуле потоков,
//           загрузка в GPU в главном потоке с бюджетом на кадр.
class AssetManager {
public:
    explicit AssetManager(std::filesystem::path root) : root_{std::move(root)} {}

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }

    // "characters/fighter_red/moves.json" → полный путь.
    [[nodiscard]] std::filesystem::path resolve(std::string_view relativePath) const {
        return root_ / std::filesystem::path{relativePath};
    }

    void unloadScope(AssetScope scope);  // TODO(§9)

private:
    std::filesystem::path root_;
};

}  // namespace dc::assets
