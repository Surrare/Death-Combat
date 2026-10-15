#pragma once

#include "gfx/gl_object.h"
#include "gfx/shader.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace dc::gfx {

// Отладочные линии и прямоугольники (хитбоксы, пол, стены арены). Образец работы с GL 4.6:
// DSA-буфер, VAO без glBind ради настройки, шейдер из assets/shaders/debug.
class DebugDraw {
public:
    DebugDraw();

    void addLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color);
    void addRect(const glm::vec2& min, const glm::vec2& max, const glm::vec4& color);

    // Рисует всё накопленное и очищает список.
    void flush(const glm::mat4& viewProjection);

private:
    struct Vertex {
        glm::vec3 position;
        glm::vec4 color;
    };

    void ensureCapacity(std::size_t vertexCount);

    std::vector<Vertex> vertices_;
    ShaderProgram program_;
    GlVertexArray vertexArray_;
    GlBuffer vertexBuffer_;
    std::size_t capacity_{0};
};

}  // namespace dc::gfx
