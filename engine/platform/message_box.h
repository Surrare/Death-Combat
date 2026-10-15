#pragma once

#include <string_view>

namespace dc::platform {

// Окно с текстом фатальной ошибки (UTF-8): игрок увидит причину, даже если консоль скрыта.
void showFatalError(std::string_view utf8Message);

}  // namespace dc::platform
