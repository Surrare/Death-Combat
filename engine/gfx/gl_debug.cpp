#include "gfx/gl_debug.h"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <windows.h>

namespace dc::gfx {
namespace {

void GLAD_API_PTR onGlMessage(GLenum, GLenum, GLuint id, GLenum severity, GLsizei, const GLchar* message,
                              const void*) {
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        spdlog::error("GL #{}: {}", id, message);
        if (IsDebuggerPresent() != FALSE) {
            __debugbreak();
        }
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        spdlog::warn("GL #{}: {}", id, message);
        break;
    default:
        spdlog::debug("GL #{}: {}", id, message);
        break;
    }
}

}  // namespace

void installDebugOutput() {
    GLint flags{0};
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if ((flags & GL_CONTEXT_FLAG_DEBUG_BIT) == 0) {
        return;
    }
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);  // ошибка останавливает отладчик ровно на виноватом вызове
    glDebugMessageCallback(onGlMessage, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
    spdlog::info("GL debug output включён");
}

}  // namespace dc::gfx
