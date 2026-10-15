#pragma once

#include <source_location>

namespace dc::core {

// Нарушен инвариант: пишем в лог, останавливаем отладчик и завершаем программу.
[[noreturn]] void assertFailed(const char* expression, const std::source_location& location) noexcept;

}  // namespace dc::core

// DC_ASSERT проверяет инварианты в Debug и исчезает в Release (docs/ARCHITECTURE.md §12.3).
// Для ошибок, которые могут случиться у игрока (нет файла, битый JSON), — исключения из core/errors.h.
#ifdef NDEBUG
#define DC_ASSERT(expr) static_cast<void>(0)
#else
#define DC_ASSERT(expr) \
    ((expr) ? static_cast<void>(0) : ::dc::core::assertFailed(#expr, std::source_location::current()))
#endif
