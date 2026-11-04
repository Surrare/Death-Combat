#pragma once
#include <string>
#include <string_view>

namespace dc {

enum class Language : int { English, Russian, Kazakh };

std::string localizeText(std::string_view text, Language language);
const char* languageName(Language language);

} // namespace dc
