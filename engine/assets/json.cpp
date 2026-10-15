#include "assets/json.h"

#include "core/file_io.h"

namespace dc::assets {

nlohmann::json loadJson(const std::filesystem::path& path) {
    const std::string text = core::readTextFile(path);
    try {
        return nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error& error) {
        throw core::DataError{core::pathToUtf8(path) + ": " + error.what()};
    }
}

}  // namespace dc::assets
