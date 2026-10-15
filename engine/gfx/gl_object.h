#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <span>
#include <utility>

namespace dc::gfx {

// RAII-обёртка над GL-объектом: только перемещение, удаление в деструкторе
// (docs/ARCHITECTURE.md §8.2; Core Guidelines R.1, C.21). Голых glGen*/glDelete* в коде игры нет.
template <auto DeleteFn>
class GlObject {
public:
    GlObject() noexcept = default;
    explicit GlObject(GLuint id) noexcept : id_{id} {}
    ~GlObject() { reset(); }

    GlObject(const GlObject&) = delete;
    GlObject& operator=(const GlObject&) = delete;
    GlObject(GlObject&& other) noexcept : id_{std::exchange(other.id_, 0)} {}
    GlObject& operator=(GlObject&& other) noexcept {
        if (this != &other) {
            reset();
            id_ = std::exchange(other.id_, 0);
        }
        return *this;
    }

    [[nodiscard]] GLuint id() const noexcept { return id_; }
    [[nodiscard]] explicit operator bool() const noexcept { return id_ != 0; }

    void reset() noexcept {
        if (id_ != 0) {
            DeleteFn(id_);
            id_ = 0;
        }
    }

private:
    GLuint id_{0};
};

inline void deleteBuffer(GLuint id) noexcept { glDeleteBuffers(1, &id); }
inline void deleteTexture(GLuint id) noexcept { glDeleteTextures(1, &id); }
inline void deleteFramebuffer(GLuint id) noexcept { glDeleteFramebuffers(1, &id); }
inline void deleteVertexArray(GLuint id) noexcept { glDeleteVertexArrays(1, &id); }
inline void deleteShader(GLuint id) noexcept { glDeleteShader(id); }
inline void deleteProgram(GLuint id) noexcept { glDeleteProgram(id); }

using GlBuffer = GlObject<&deleteBuffer>;
using GlTexture = GlObject<&deleteTexture>;
using GlFramebuffer = GlObject<&deleteFramebuffer>;
using GlVertexArray = GlObject<&deleteVertexArray>;
using GlShader = GlObject<&deleteShader>;
using GlProgram = GlObject<&deleteProgram>;

// Создание через DSA (glCreate* + glNamed*): объект готов сразу, без glBind* ради редактирования.
[[nodiscard]] inline GlBuffer createBuffer(std::size_t sizeBytes, const void* initialData, GLbitfield flags) {
    GLuint id{0};
    glCreateBuffers(1, &id);
    glNamedBufferStorage(id, static_cast<GLsizeiptr>(sizeBytes), initialData, flags);
    return GlBuffer{id};
}

[[nodiscard]] inline GlBuffer createBuffer(std::span<const std::byte> data, GLbitfield flags) {
    return createBuffer(data.size(), data.data(), flags);
}

}  // namespace dc::gfx
