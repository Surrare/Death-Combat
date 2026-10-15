#include "gfx/debug_draw.h"

#include "core/file_io.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstddef>

namespace dc::gfx {
namespace {

constexpr std::size_t initialVertexCapacity = 4096;
constexpr GLuint viewProjectionLocation = 0;  // layout(location = 0) uniform mat4 в lines.vert

}  // namespace

DebugDraw::DebugDraw()
    : program_{ShaderProgram::fromFiles(core::assetsDirectory() / "shaders/debug/lines.vert",
                                        core::assetsDirectory() / "shaders/debug/lines.frag")} {
    GLuint vertexArray{0};
    glCreateVertexArrays(1, &vertexArray);
    vertexArray_ = GlVertexArray{vertexArray};

    glEnableVertexArrayAttrib(vertexArray, 0);
    glVertexArrayAttribFormat(vertexArray, 0, 3, GL_FLOAT, GL_FALSE, static_cast<GLuint>(offsetof(Vertex, position)));
    glVertexArrayAttribBinding(vertexArray, 0, 0);
    glEnableVertexArrayAttrib(vertexArray, 1);
    glVertexArrayAttribFormat(vertexArray, 1, 4, GL_FLOAT, GL_FALSE, static_cast<GLuint>(offsetof(Vertex, color)));
    glVertexArrayAttribBinding(vertexArray, 1, 0);
    glObjectLabel(GL_VERTEX_ARRAY, vertexArray, -1, "DebugDraw.vao");

    ensureCapacity(initialVertexCapacity);
}

void DebugDraw::addLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color) {
    vertices_.push_back({from, color});
    vertices_.push_back({to, color});
}

void DebugDraw::addRect(const glm::vec2& min, const glm::vec2& max, const glm::vec4& color) {
    const glm::vec3 a{min.x, min.y, 0.0f};
    const glm::vec3 b{max.x, min.y, 0.0f};
    const glm::vec3 c{max.x, max.y, 0.0f};
    const glm::vec3 d{min.x, max.y, 0.0f};
    addLine(a, b, color);
    addLine(b, c, color);
    addLine(c, d, color);
    addLine(d, a, color);
}

void DebugDraw::flush(const glm::mat4& viewProjection) {
    if (vertices_.empty()) {
        return;
    }
    ensureCapacity(vertices_.size());
    glNamedBufferSubData(vertexBuffer_.id(), 0, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                         vertices_.data());

    glUseProgram(program_.id());
    glProgramUniformMatrix4fv(program_.id(), viewProjectionLocation, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glBindVertexArray(vertexArray_.id());
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices_.size()));
    vertices_.clear();
}

void DebugDraw::ensureCapacity(std::size_t vertexCount) {
    if (vertexCount <= capacity_) {
        return;
    }
    // TODO(§8.2): persistent mapped кольцевой буфер с glFenceSync вместо пересоздания и glNamedBufferSubData.
    capacity_ = std::max(vertexCount, capacity_ * 2);
    vertexBuffer_ = createBuffer(capacity_ * sizeof(Vertex), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glVertexArrayVertexBuffer(vertexArray_.id(), 0, vertexBuffer_.id(), 0, static_cast<GLsizei>(sizeof(Vertex)));
    glObjectLabel(GL_BUFFER, vertexBuffer_.id(), -1, "DebugDraw.vbo");
}

}  // namespace dc::gfx
