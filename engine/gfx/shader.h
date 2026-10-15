#pragma once

#include "gfx/gl_object.h"

#include <filesystem>

namespace dc::gfx {

// Программа из вершинного и фрагментного шейдеров (GLSL 460).
// TODO(§8.3): препроцессор #include, варианты через #define (SKINNED, LIGHTMAPPED), горячая перезагрузка в Debug.
class ShaderProgram {
public:
    // GlError с логом компилятора, если шейдер не собрался.
    [[nodiscard]] static ShaderProgram fromFiles(const std::filesystem::path& vertexPath,
                                                 const std::filesystem::path& fragmentPath);

    [[nodiscard]] GLuint id() const noexcept { return program_.id(); }

private:
    explicit ShaderProgram(GlProgram program) noexcept : program_{std::move(program)} {}

    GlProgram program_;
};

}  // namespace dc::gfx
