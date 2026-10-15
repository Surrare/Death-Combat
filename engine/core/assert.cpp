#include "core/assert.h"

#include <spdlog/spdlog.h>
#include <windows.h>

#include <cstdlib>

namespace dc::core {

void assertFailed(const char* expression, const std::source_location& location) noexcept {
    spdlog::critical("DC_ASSERT({}) не выполнен: {}:{} ({})", expression, location.file_name(), location.line(),
                     location.function_name());
    spdlog::shutdown();
    if (IsDebuggerPresent() != FALSE) {
        __debugbreak();
    }
    std::abort();
}

}  // namespace dc::core
