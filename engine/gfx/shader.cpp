#include "gfx/shader.h"

#include "core/errors.h"
#include "core/file_io.h"

#include <string>

namespace dc::gfx {
namespace {

std::string shaderLog(GLuint shader) {
    GLint length{0};
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(length), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return log;
}

std::string programLog(GLuint program) {
    GLint length{0};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(length), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    return log;
}

GlShader compileShader(GLenum stage, const std::filesystem::path& path) {
    const std::string source = core::readTextFile(path);
    const char* text = source.c_str();

    GlShader shader{glCreateShader(stage)};
    glShaderSource(shader.id(), 1, &text, nullptr);
    glCompileShader(shader.id());

    GLint compiled{GL_FALSE};
    glGetShaderiv(shader.id(), GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        throw core::GlError{"Шейдер " + core::pathToUtf8(path) + " не скомпилировался:\n" + shaderLog(shader.id())};
    }

    const std::string label = core::pathToUtf8(path.filename());
    glObjectLabel(GL_SHADER, shader.id(), -1, label.c_str());
    return shader;
}

}  // namespace

ShaderProgram ShaderProgram::fromFiles(const std::filesystem::path& vertexPath,
                                       const std::filesystem::path& fragmentPath) {
    const GlShader vertex = compileShader(GL_VERTEX_SHADER, vertexPath);
    const GlShader fragment = compileShader(GL_FRAGMENT_SHADER, fragmentPath);

    GlProgram program{glCreateProgram()};
    glAttachShader(program.id(), vertex.id());
    glAttachShader(program.id(), fragment.id());
    glLinkProgram(program.id());
    glDetachShader(program.id(), vertex.id());
    glDetachShader(program.id(), fragment.id());

    GLint linked{GL_FALSE};
    glGetProgramiv(program.id(), GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        throw core::GlError{"Программа " + core::pathToUtf8(vertexPath.stem()) + " не слинковалась:\n" +
                            programLog(program.id())};
    }

    const std::string label = core::pathToUtf8(vertexPath.stem());
    glObjectLabel(GL_PROGRAM, program.id(), -1, label.c_str());
    return ShaderProgram{std::move(program)};
}

}  // namespace dc::gfx
