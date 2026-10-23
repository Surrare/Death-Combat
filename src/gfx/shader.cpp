#include "gfx/shader.h"
#include <cstdio>
#include <vector>

namespace dc {

static GLuint compileStage(GLenum type, const char* src, const char* debugName) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] %s (%s) compile error:\n%s\n", debugName,
                     type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

bool Shader::compile(const char* vertexSrc, const char* fragmentSrc, const char* debugName) {
    GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc, debugName);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc, debugName);
    if (!vs || !fs) return false;
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] %s link error:\n%s\n", debugName, log);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    return true;
}

GLint Shader::location(const char* name) {
    auto it = locations_.find(name);
    if (it != locations_.end()) return it->second;
    GLint loc = glGetUniformLocation(program_, name);
    locations_[name] = loc;
    return loc;
}

void Shader::set(const char* n, int v) { glUniform1i(location(n), v); }
void Shader::set(const char* n, float v) { glUniform1f(location(n), v); }
void Shader::set(const char* n, const Vec2& v) { glUniform2f(location(n), v.x, v.y); }
void Shader::set(const char* n, const Vec3& v) { glUniform3f(location(n), v.x, v.y, v.z); }
void Shader::set(const char* n, const Vec4& v) { glUniform4f(location(n), v.x, v.y, v.z, v.w); }
void Shader::set(const char* n, const Color& c) { glUniform4f(location(n), c.r, c.g, c.b, c.a); }
void Shader::set(const char* n, const Mat4& m) { glUniformMatrix4fv(location(n), 1, GL_FALSE, m.data()); }
void Shader::setArray(const char* n, const Vec3* v, int count) { glUniform3fv(location(n), count, &v[0].x); }
void Shader::setArray(const char* n, const Vec4* v, int count) { glUniform4fv(location(n), count, &v[0].x); }

} // namespace dc
