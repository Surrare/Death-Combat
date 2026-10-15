#include "platform/message_box.h"

#include <windows.h>

#include <string>

namespace dc::platform {

void showFatalError(std::string_view utf8Message) {
    const int sourceLength = static_cast<int>(utf8Message.size());
    const int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8Message.data(), sourceLength, nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Message.data(), sourceLength, wide.data(), wideLength);
    MessageBoxW(nullptr, wide.c_str(), L"Death Combat", MB_ICONERROR | MB_OK);
}

}  // namespace dc::platform
